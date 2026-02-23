#include "PluginAPI.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace swg::plugin;

namespace
{
    // Very small JSON representation tailored for scenario ingestion.
    struct JsonValue
    {
        enum class Type
        {
            Null,
            Boolean,
            Number,
            String,
            Array,
            Object
        };

        JsonValue() = default;
        JsonValue(const JsonValue &) = default;
        JsonValue(JsonValue &&other)
            : type(other.type),
              boolean(other.boolean),
              number(other.number),
              string(std::move(other.string)),
              array(std::move(other.array)),
              object(std::move(other.object))
        {
        }

        JsonValue &operator=(const JsonValue &) = default;
        JsonValue &operator=(JsonValue &&other)
        {
            if (this != &other)
            {
                type = other.type;
                boolean = other.boolean;
                number = other.number;
                string = std::move(other.string);
                array = std::move(other.array);
                object = std::move(other.object);
            }
            return *this;
        }

        Type type = Type::Null;
        bool boolean = false;
        double number = 0.0;
        std::string string;
        std::vector<JsonValue> array;
        std::unordered_map<std::string, JsonValue> object;
    };

    struct JsonParser
    {
        explicit JsonParser(const std::string &source) : text(source) {}

        bool parse(JsonValue &out, std::string &error)
        {
            skipWhitespace();
            if (!parseValue(out, error))
                return false;
            skipWhitespace();
            if (position != text.size())
            {
                error = "Unexpected trailing characters in JSON";
                return false;
            }
            return true;
        }

    private:
        const std::string &text;
        std::size_t position = 0;

        bool startsWith(const char *literal) const
        {
            const std::size_t length = std::strlen(literal);
            return position + length <= text.size() && std::memcmp(text.data() + position, literal, length) == 0;
        }

        void skipWhitespace()
        {
            while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position])))
            {
                ++position;
            }
        }

        bool match(char expected)
        {
            skipWhitespace();
            if (position < text.size() && text[position] == expected)
            {
                ++position;
                return true;
            }
            return false;
        }

        bool parseValue(JsonValue &out, std::string &error)
        {
            skipWhitespace();
            if (position >= text.size())
            {
                error = "Unexpected end of JSON input";
                return false;
            }

            const char ch = text[position];
            if (ch == '"')
                return parseString(out, error);
            if (ch == '-' || (ch >= '0' && ch <= '9'))
                return parseNumber(out, error);
            if (ch == '{')
                return parseObject(out, error);
            if (ch == '[')
                return parseArray(out, error);
            if (startsWith("true"))
            {
                JsonValue value;
                value.type = JsonValue::Type::Boolean;
                value.boolean = true;
                return parseLiteral(out, "true", value);
            }
            if (startsWith("false"))
            {
                JsonValue value;
                value.type = JsonValue::Type::Boolean;
                value.boolean = false;
                return parseLiteral(out, "false", value);
            }
            if (startsWith("null"))
                return parseLiteral(out, "null", JsonValue{});

            error = "Unrecognized token in JSON";
            return false;
        }

        bool parseLiteral(JsonValue &out, const char *literal, JsonValue value)
        {
            position += std::strlen(literal);
            out = std::move(value);
            return true;
        }

        bool parseString(JsonValue &out, std::string &error)
        {
            if (!match('"'))
            {
                error = "Expected opening quote for JSON string";
                return false;
            }

            std::string result;
            bool foundClosingQuote = false;
            while (position < text.size())
            {
                char ch = text[position++];
                if (ch == '"')
                {
                    foundClosingQuote = true;
                    break;
                }
                if (ch == '\\')
                {
                    if (position >= text.size())
                        break;
                    char esc = text[position++];
                    switch (esc)
                    {
                    case '"': result.push_back('"'); break;
                    case '\\': result.push_back('\\'); break;
                    case '/': result.push_back('/'); break;
                    case 'b': result.push_back('\b'); break;
                    case 'f': result.push_back('\f'); break;
                    case 'n': result.push_back('\n'); break;
                    case 'r': result.push_back('\r'); break;
                    case 't': result.push_back('\t'); break;
                    default: result.push_back(esc); break;
                    }
                }
                else
                {
                    result.push_back(ch);
                }
            }

            if (!foundClosingQuote)
            {
                position = std::min(position, text.size());
                error = "Unterminated JSON string";
                return false;
            }

            out.type = JsonValue::Type::String;
            out.string = std::move(result);
            return true;
        }

        bool parseNumber(JsonValue &out, std::string &error)
        {
            const std::size_t start = position;
            if (text[position] == '-')
                ++position;
            while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position])))
                ++position;
            if (position < text.size() && text[position] == '.')
            {
                ++position;
                while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position])))
                    ++position;
            }

            const std::string slice(text.data() + start, position - start);
            char *endPtr = nullptr;
            const double value = std::strtod(slice.c_str(), &endPtr);
            if (!endPtr || endPtr == slice.c_str())
            {
                error = "Failed to parse number in JSON";
                return false;
            }

            out.type = JsonValue::Type::Number;
            out.number = value;
            return true;
        }

        bool parseArray(JsonValue &out, std::string &error)
        {
            if (!match('['))
            {
                error = "Expected '[' when parsing JSON array";
                return false;
            }

            out.type = JsonValue::Type::Array;
            out.array.clear();

            skipWhitespace();
            if (position < text.size() && text[position] == ']')
            {
                ++position;
                return true;
            }

            while (position < text.size())
            {
                JsonValue element;
                if (!parseValue(element, error))
                    return false;
                out.array.push_back(std::move(element));

                skipWhitespace();
                if (position < text.size() && text[position] == ',')
                {
                    ++position;
                    continue;
                }
                if (position < text.size() && text[position] == ']')
                {
                    ++position;
                    return true;
                }
                error = "Expected ',' or ']' after array element";
                return false;
            }

            error = "Unterminated array in JSON";
            return false;
        }

        bool parseObject(JsonValue &out, std::string &error)
        {
            if (!match('{'))
            {
                error = "Expected '{' when parsing JSON object";
                return false;
            }

            out.type = JsonValue::Type::Object;
            out.object.clear();

            skipWhitespace();
            if (position < text.size() && text[position] == '}')
            {
                ++position;
                return true;
            }

            while (position < text.size())
            {
                JsonValue key;
                if (!parseString(key, error))
                    return false;
                if (!match(':'))
                {
                    error = "Expected ':' after object key";
                    return false;
                }

                JsonValue value;
                if (!parseValue(value, error))
                    return false;

                out.object.emplace(std::move(key.string), std::move(value));

                skipWhitespace();
                if (position < text.size() && text[position] == ',')
                {
                    ++position;
                    skipWhitespace();
                    continue;
                }
                if (position < text.size() && text[position] == '}')
                {
                    ++position;
                    return true;
                }

                error = "Expected ',' or '}' after object member";
                return false;
            }

            error = "Unterminated object in JSON";
            return false;
        }
    };

    struct Account
    {
        std::string username;
        std::string password;
        std::string character;
    };

    struct SpawnPoint
    {
        std::string planet;
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    struct Scenario
    {
        std::vector<Account> accounts;
        std::vector<SpawnPoint> spawns;
        std::vector<std::string> behaviors;
        double connectRatePerSecond = 1.0;
        double pingIntervalSeconds = 5.0;
    };

    struct Agent
    {
        Account account;
        SpawnPoint spawn;
        std::size_t behaviorIndex = 0;
        double timeSinceLastAction = 0.0;
        double timeSincePing = 0.0;
        bool connecting = true;
        bool authenticated = false;
        bool active = true;
        double simulatedLatencyMs = 50.0;
    };

    struct Metrics
    {
        std::size_t attemptedConnections = 0;
        std::size_t successfulConnections = 0;
        std::size_t loginFailures = 0;
        double totalLatencyMs = 0.0;
        std::size_t latencySamples = 0;
        double elapsed = 0.0;
        double lastLog = 0.0;
    };

    struct PluginState
    {
        HostContext host{};
        Scenario scenario{};
        bool scenarioLoaded = false;
        bool scenarioRunning = false;
        std::string scenarioPath;
        std::vector<Agent> agents;
        Metrics metrics{};
        double spawnAccumulator = 0.0;
        std::string activeScenarioPath;
    };

    PluginState g_state{};

    void logMessage(LogLevel level, const std::string &message)
    {
        if (g_state.host.dispatch.log)
        {
            g_state.host.dispatch.log(level, StringView{message.c_str(), message.size()});
        }
    }

    bool readFile(const std::string &path, std::string &contents)
    {
        std::ifstream file(path.c_str());
        if (!file)
            return false;
        std::ostringstream buffer;
        buffer << file.rdbuf();
        contents = buffer.str();
        return true;
    }

    bool parseScenario(const std::string &path, Scenario &scenario, std::string &error)
    {
        std::string contents;
        if (!readFile(path, contents))
        {
            error = "Unable to open scenario file: " + path;
            return false;
        }

        JsonParser parser(contents);
        JsonValue root;
        if (!parser.parse(root, error))
            return false;

        if (root.type != JsonValue::Type::Object)
        {
            error = "Scenario root must be an object";
            return false;
        }

        Scenario parsed;

        auto accountsIt = root.object.find("accounts");
        if (accountsIt != root.object.end() && accountsIt->second.type == JsonValue::Type::Array)
        {
            for (const auto &entry : accountsIt->second.array)
            {
                if (entry.type != JsonValue::Type::Object)
                    continue;
                Account acc;
                auto u = entry.object.find("username");
                auto p = entry.object.find("password");
                auto c = entry.object.find("character");
                if (u != entry.object.end() && u->second.type == JsonValue::Type::String)
                    acc.username = u->second.string;
                if (p != entry.object.end() && p->second.type == JsonValue::Type::String)
                    acc.password = p->second.string;
                if (c != entry.object.end() && c->second.type == JsonValue::Type::String)
                    acc.character = c->second.string;
                parsed.accounts.push_back(std::move(acc));
            }
        }

        auto spawnsIt = root.object.find("spawns");
        if (spawnsIt != root.object.end() && spawnsIt->second.type == JsonValue::Type::Array)
        {
            for (const auto &entry : spawnsIt->second.array)
            {
                if (entry.type != JsonValue::Type::Object)
                    continue;
                SpawnPoint spawn;
                auto planet = entry.object.find("planet");
                auto x = entry.object.find("x");
                auto y = entry.object.find("y");
                auto z = entry.object.find("z");
                if (planet != entry.object.end() && planet->second.type == JsonValue::Type::String)
                    spawn.planet = planet->second.string;
                if (x != entry.object.end() && x->second.type == JsonValue::Type::Number)
                    spawn.x = x->second.number;
                if (y != entry.object.end() && y->second.type == JsonValue::Type::Number)
                    spawn.y = y->second.number;
                if (z != entry.object.end() && z->second.type == JsonValue::Type::Number)
                    spawn.z = z->second.number;
                parsed.spawns.push_back(std::move(spawn));
            }
        }

        auto behaviorsIt = root.object.find("behaviors");
        if (behaviorsIt != root.object.end() && behaviorsIt->second.type == JsonValue::Type::Array)
        {
            for (const auto &entry : behaviorsIt->second.array)
            {
                if (entry.type == JsonValue::Type::String)
                    parsed.behaviors.push_back(entry.string);
            }
        }

        auto rateIt = root.object.find("connectRatePerSecond");
        if (rateIt != root.object.end() && rateIt->second.type == JsonValue::Type::Number)
        {
            parsed.connectRatePerSecond = std::max(0.1, rateIt->second.number);
        }

        auto pingIt = root.object.find("pingIntervalSeconds");
        if (pingIt != root.object.end() && pingIt->second.type == JsonValue::Type::Number)
        {
            parsed.pingIntervalSeconds = std::max(1.0, pingIt->second.number);
        }

        if (parsed.accounts.empty())
        {
            error = "Scenario must include at least one account entry";
            return false;
        }

        if (parsed.spawns.empty())
        {
            SpawnPoint defaultSpawn;
            defaultSpawn.planet = "unknown";
            parsed.spawns.push_back(defaultSpawn);
        }

        if (parsed.behaviors.empty())
        {
            parsed.behaviors.push_back("idle");
        }

        scenario = parsed;
        return true;
    }

    SpawnPoint chooseSpawn(const Scenario &scenario, std::size_t index)
    {
        if (scenario.spawns.empty())
            return SpawnPoint{};
        return scenario.spawns[index % scenario.spawns.size()];
    }

    void emitMetrics()
    {
        if (!g_state.host.dispatch.log)
            return;

        std::ostringstream out;
        out << "[ai_load_tester] cps=";
        const double cps = g_state.metrics.elapsed > 0.0
            ? static_cast<double>(g_state.metrics.attemptedConnections) / g_state.metrics.elapsed
            : 0.0;
        out << cps;
        out << " success=" << g_state.metrics.successfulConnections;
        out << " login_failures=" << g_state.metrics.loginFailures;
        const double avgLatency = g_state.metrics.latencySamples > 0
            ? g_state.metrics.totalLatencyMs / static_cast<double>(g_state.metrics.latencySamples)
            : 0.0;
        out << " avg_latency_ms=" << avgLatency;
        out << " active_agents=" << g_state.agents.size();

        logMessage(LogLevel::Info, out.str());
    }

    void logScenarioSummary()
    {
        std::ostringstream out;
        out << "AI load scenario ready (path='" << g_state.activeScenarioPath << "'";
        out << ", accounts=" << g_state.scenario.accounts.size();
        out << ", spawns=" << g_state.scenario.spawns.size();
        out << ", behaviors=" << g_state.scenario.behaviors.size();
        out << ", connect_rate_per_second=" << g_state.scenario.connectRatePerSecond;
        out << ", ping_interval_seconds=" << g_state.scenario.pingIntervalSeconds;
        out << ")";

        logMessage(LogLevel::Info, out.str());
    }

    void resetMetrics()
    {
        g_state.metrics = Metrics{};
        g_state.spawnAccumulator = 0.0;
    }

    bool loadScenarioFromDisk(std::string &pathUsed, std::string &error, Scenario &scenario)
    {
        const char *kDefaultPath = "plugin/ai_load_tester/scenario.json";
        const char *kSamplePath = "plugin/ai_load_tester/scenario.sample.json";

        std::vector<std::string> candidates;
        candidates.push_back(g_state.scenarioPath);

        if (g_state.scenarioPath != kSamplePath)
            candidates.emplace_back(kSamplePath);
        if (g_state.scenarioPath != kDefaultPath)
            candidates.emplace_back(std::string(kDefaultPath));

        for (const auto &candidate : candidates)
        {
            std::string parseError;
            Scenario parsedScenario;
            if (parseScenario(candidate, parsedScenario, parseError))
            {
                pathUsed = candidate;
                scenario = parsedScenario;
                return true;
            }

            error = parseError;
        }

        return false;
    }

    void stopScenario()
    {
        if (!g_state.scenarioRunning)
            return;

        g_state.scenarioRunning = false;
        g_state.agents.clear();
        emitMetrics();
        logMessage(LogLevel::Info, "AI load scenario stopped");
    }

    void startScenarioInternal();

    void startScenario(const std::string &preferredPath)
    {
        if (!preferredPath.empty())
            g_state.scenarioPath = preferredPath;

        startScenarioInternal();
    }

    void startScenarioInternal()
    {
        if (g_state.scenarioRunning)
        {
            stopScenario();
        }

        std::string error;
        std::string pathUsed;
        Scenario scenario;
        if (!loadScenarioFromDisk(pathUsed, error, scenario))
        {
            logMessage(LogLevel::Error, "Failed to start AI load scenario: " + error);
            return;
        }

        g_state.scenario = scenario;
        g_state.scenarioLoaded = true;
        g_state.scenarioRunning = true;
        g_state.agents.clear();
        resetMetrics();
        g_state.activeScenarioPath = pathUsed;

        if (!pathUsed.empty() && pathUsed != g_state.scenarioPath)
        {
            logMessage(LogLevel::Warn, "Scenario not found at preferred path; using fallback: " + pathUsed);
        }

        logMessage(LogLevel::Info, "AI load scenario started using " + g_state.activeScenarioPath);
        logScenarioSummary();
    }

    void startScenarioCommand(void *)
    {
        if (!g_state.host.dispatch.enqueueTask)
        {
            logMessage(LogLevel::Error, "Host does not support enqueueTask; cannot start AI load scenario");
            return;
        }

        g_state.host.dispatch.enqueueTask(
            [](void *) { startScenarioInternal(); },
            nullptr);
    }

    void stopScenarioCommand(void *)
    {
        if (!g_state.host.dispatch.enqueueTask)
        {
            stopScenario();
            return;
        }

        g_state.host.dispatch.enqueueTask(
            [](void *) { stopScenario(); },
            nullptr);
    }

    void statusScenarioCommand(void *)
    {
        if (!g_state.host.dispatch.enqueueTask)
        {
            if (!g_state.scenarioRunning)
            {
                logMessage(LogLevel::Info, "AI load scenario is idle");
            }
            else
            {
                emitMetrics();
            }
            return;
        }

        g_state.host.dispatch.enqueueTask(
            [](void *) {
                if (!g_state.scenarioRunning)
                {
                    logMessage(LogLevel::Info, "AI load scenario is idle");
                    return;
                }
                emitMetrics();
            },
            nullptr);
    }

    void onTick(double deltaSeconds)
    {
        if (!g_state.scenarioRunning)
            return;

        g_state.metrics.elapsed += deltaSeconds;
        g_state.metrics.lastLog += deltaSeconds;

        g_state.spawnAccumulator += deltaSeconds * g_state.scenario.connectRatePerSecond;
        while (g_state.spawnAccumulator >= 1.0 && g_state.agents.size() < g_state.scenario.accounts.size())
        {
            const std::size_t index = g_state.agents.size();
            Agent agent;
            agent.account = g_state.scenario.accounts[index];
            agent.spawn = chooseSpawn(g_state.scenario, index);
            g_state.agents.push_back(std::move(agent));
            g_state.metrics.attemptedConnections++;
            g_state.spawnAccumulator -= 1.0;
        }

        for (auto &agent : g_state.agents)
        {
            if (!agent.active)
                continue;

            agent.timeSinceLastAction += deltaSeconds;
            agent.timeSincePing += deltaSeconds;

            if (agent.connecting)
            {
                if (agent.account.password.empty())
                {
                    agent.active = false;
                    ++g_state.metrics.loginFailures;
                    continue;
                }

                if (agent.timeSinceLastAction >= 0.5)
                {
                    agent.connecting = false;
                    agent.authenticated = true;
                    agent.timeSinceLastAction = 0.0;
                    ++g_state.metrics.successfulConnections;
                }
                continue;
            }

            if (agent.timeSincePing >= g_state.scenario.pingIntervalSeconds)
            {
                agent.timeSincePing = 0.0;
                g_state.metrics.totalLatencyMs += agent.simulatedLatencyMs;
                ++g_state.metrics.latencySamples;
            }

            if (!g_state.scenario.behaviors.empty() && agent.timeSinceLastAction >= 1.0)
            {
                agent.behaviorIndex = (agent.behaviorIndex + 1) % g_state.scenario.behaviors.size();
                agent.timeSinceLastAction = 0.0;
            }
        }

        if (g_state.metrics.lastLog >= 5.0)
        {
            emitMetrics();
            g_state.metrics.lastLog = 0.0;
        }
    }

    bool onLoad(const HostContext &context)
    {
        g_state.host = context;
        const char *defaultPath = std::getenv("SWG_AI_LOAD_SCENARIO");
        g_state.scenarioPath = defaultPath ? defaultPath : "plugin/ai_load_tester/Swg+ai.cfg";
        g_state.activeScenarioPath = g_state.scenarioPath;

        if (context.dispatch.registerCommand)
        {
            context.dispatch.registerCommand(StringView{"ai_load_start", std::strlen("ai_load_start")}, &startScenarioCommand, nullptr);
            context.dispatch.registerCommand(StringView{"ai_load_stop", std::strlen("ai_load_stop")}, &stopScenarioCommand, nullptr);
            context.dispatch.registerCommand(StringView{"ai_load_status", std::strlen("ai_load_status")}, &statusScenarioCommand, nullptr);
        }

        logMessage(LogLevel::Info, "AI Load Tester plugin loaded");

        // Auto-start when a scenario configuration is present so load tests can begin
        // as soon as the client initializes. This keeps headless testing fully
        // configuration-driven through the Swg+ai.cfg file without requiring manual
        // commands.
        startScenario(g_state.scenarioPath);
        return true;
    }

    void onUnload()
    {
        stopScenario();
        logMessage(LogLevel::Info, "AI Load Tester plugin unloaded");
    }
}

extern "C" bool SwgRegisterPlugin(const HostContext &context, PluginDescriptor &descriptor, Lifecycle &lifecycle)
{
    descriptor.name = StringView{"AiLoadTester", std::strlen("AiLoadTester")};
    descriptor.description = StringView{"Headless AI controller for exercising login and scripted loops.", std::strlen("Headless AI controller for exercising login and scripted loops.")};
    descriptor.pluginVersion = makeVersion(0, 1, 0);
    descriptor.compatibleApiMin = makeVersion(1, 0, 0);
    descriptor.compatibleApiMax = makeVersion(1, 0, 0);

    lifecycle.onLoad = &onLoad;
    lifecycle.onUnload = &onUnload;
    lifecycle.onTick = &onTick;

    return lifecycle.onLoad ? lifecycle.onLoad(context) : false;
}

