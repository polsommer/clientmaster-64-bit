-- Star Wars Galaxies Exporter for Autodesk 3ds Max (all supported versions)
-- ------------------------------------------------------
-- This MaxScript replicates the Star Wars Galaxies Maya exporter workflow in 3ds Max.
-- It provides rollout UI, validation, configuration persistence, export automation,
-- and publishing hooks so artists can build game-ready assets directly from Max.
--
-- The exporter relies on native 3ds Max capabilities for geometry extraction
-- and uses the SWG command line toolchain (swg-tool) for final asset conversion.
-- ------------------------------------------------------

global SWGExporter
global SWG_AllowUnsupportedMaxVersion
global SWG_SupportedMaxYearMin
global SWG_SupportedMaxYearMax
global SWG_MaxVersionInfo

SWG_AllowUnsupportedMaxVersion = true
SWG_SupportedMaxYearMin = 1900
SWG_SupportedMaxYearMax = 9999

fn swg_getMaxVersionInfo =
(
    local major = undefined
    local year = undefined
    local productVersion = undefined

    try
    (
        local versionData = maxVersion()
        if versionData != undefined and versionData.count > 0 do major = versionData[1]
    )
    catch()

    try(productVersion = maxProductVersion()) catch()

    if productVersion != undefined then
    (
        local tokens = filterString (productVersion as string) "."
        if tokens.count > 0 then
        (
            local parsedYear = undefined
            try(parsedYear = (tokens[1] as integer)) catch(parsedYear = undefined)
            if parsedYear != undefined and parsedYear > 2000 do year = parsedYear
        )
    )

    if year == undefined and major != undefined do year = major + 1998
    #(year, major, productVersion)
)

fn swg_formatMaxVersionInfo info =
(
    local year = info[1]
    local major = info[2]
    local productVersion = info[3]
    local detail = ""
    if year != undefined do detail += (year as string)
    if major != undefined do detail += (if detail != "" then " (major " else "Major ") + (major as string) + (if detail != "" then ")" else "")
    if productVersion != undefined do detail += (if detail != "" then ", " else "") + "product " + (productVersion as string)
    if detail == "" do detail = "unknown"
    detail
)

fn swg_logMaxVersion state info =
(
    local detail = swg_formatMaxVersionInfo info
    local message = "Detected 3ds Max version: " + detail + "."
    if SWG_AllowUnsupportedMaxVersion == true do message += " Override enabled."
    swg_log state message
    format "[SWG Exporter] %\n" message
)

fn swg_isSupportedMaxVersion info =
(
    if SWG_AllowUnsupportedMaxVersion == true then
    (
        true
    )
    else
    (
        local year = info[1]
        year != undefined and year >= SWG_SupportedMaxYearMin and year <= SWG_SupportedMaxYearMax
    )
)

fn swg_checkMaxVersion =
(
    local info = swg_getMaxVersionInfo()
    SWG_MaxVersionInfo = info
    swg_logMaxVersion SWGExporter info

    if swg_isSupportedMaxVersion info then
    (
        true
    )
    else
    (
        if SWG_AllowUnsupportedMaxVersion == true then
        (
            true
        )
        else
        (
            local requiredRange = if SWG_SupportedMaxYearMin == SWG_SupportedMaxYearMax then (SWG_SupportedMaxYearMin as string) else ((SWG_SupportedMaxYearMin as string) + "-" + (SWG_SupportedMaxYearMax as string))
            local detail = swg_formatMaxVersionInfo info
            messageBox ("This exporter supports Autodesk 3ds Max " + requiredRange + ".\nDetected: " + detail + ".\n\nSet global SWG_AllowUnsupportedMaxVersion = true to override.") title:"SWG Exporter"
            false
        )
    )
)

struct SWGExporterSettings
(
    version,
    projectRoot,
    appearancePath,
    meshPath,
    skeletonPath,
    animationPath,
    shaderPath,
    collisionPath,
    logPath,
    publishPath,
    exportPreset,
    convertWithSwgTool,
    publishAfterExport
)

struct SWGExporterState
(
    rollout,
    settings,
    settingsFile,
    logFile,
    mutex = false,
    validationSummary = "Last validation: Not run."
)

fn swg_getUserScriptsDirectory =
(
    local scriptsDir = getDir #userScripts
    if scriptsDir == undefined or scriptsDir == "" do
    (
        scriptsDir = getDir #scripts
    )
    scriptsDir
)

fn swg_getSettingsFile =
(
    (swg_getUserScriptsDirectory()) + "/SWGExporterSettings.ini"
)

fn swg_defaultSettings =
(
    SWGExporterSettings 
    (
        version = "1.0.0",
        projectRoot = getDir #maxroot,
        appearancePath = getDir #export + "/appearance",
        meshPath = getDir #export + "/mesh",
        skeletonPath = getDir #export + "/skeleton",
        animationPath = getDir #export + "/animation",
        shaderPath = getDir #export + "/shader",
        collisionPath = getDir #export + "/collision",
        logPath = swg_getUserScriptsDirectory() + "/logs",
        publishPath = swg_getUserScriptsDirectory() + "/publish",
        exportPreset = "GameExporter",
        convertWithSwgTool = true,
        publishAfterExport = false
    )
)

fn swg_ensureDirectory path =
(
    if (doesFileExist path) then
    (
        if (getFileAttribute path #isdir) then true else false
    )
    else
    (
        makeDir path
    )
)

fn swg_log state message =
(
    if state.logFile == undefined do return undefined
    local dotNetDateTime = dotNetClass "System.DateTime"
    local timestamp = dotNetDateTime.Now.ToString "yyyy-MM-dd HH:mm:ss"
    local line = timestamp + " - " + message + "\n"
    local stream = openFile state.logFile mode:"at"
    if stream != undefined then
    (
        format "%" line to:stream
        close stream
    )
)

fn swg_loadSettings state =
(
    local file = state.settingsFile
    if not (doesFileExist file) then
    (
        state.settings = swg_defaultSettings()
        swg_saveSettings state
        return state.settings
    )

    local settings = swg_defaultSettings()

    settings.projectRoot = getINISetting file "paths" "projectRoot" default:settings.projectRoot
    settings.appearancePath = getINISetting file "paths" "appearancePath" default:settings.appearancePath
    settings.meshPath = getINISetting file "paths" "meshPath" default:settings.meshPath
    settings.skeletonPath = getINISetting file "paths" "skeletonPath" default:settings.skeletonPath
    settings.animationPath = getINISetting file "paths" "animationPath" default:settings.animationPath
    settings.shaderPath = getINISetting file "paths" "shaderPath" default:settings.shaderPath
    settings.collisionPath = getINISetting file "paths" "collisionPath" default:settings.collisionPath
    settings.logPath = getINISetting file "paths" "logPath" default:settings.logPath
    settings.publishPath = getINISetting file "paths" "publishPath" default:settings.publishPath
    settings.exportPreset = getINISetting file "export" "preset" default:settings.exportPreset
    settings.convertWithSwgTool = (getINISetting file "export" "convertWithSwgTool" default:(settings.convertWithSwgTool as string)) as boolean
    settings.publishAfterExport = (getINISetting file "export" "publishAfterExport" default:(settings.publishAfterExport as string)) as boolean

    state.settings = settings
    state
)

fn swg_saveSettings state =
(
    local file = state.settingsFile
    setINISetting file "paths" "projectRoot" state.settings.projectRoot
    setINISetting file "paths" "appearancePath" state.settings.appearancePath
    setINISetting file "paths" "meshPath" state.settings.meshPath
    setINISetting file "paths" "skeletonPath" state.settings.skeletonPath
    setINISetting file "paths" "animationPath" state.settings.animationPath
    setINISetting file "paths" "shaderPath" state.settings.shaderPath
    setINISetting file "paths" "collisionPath" state.settings.collisionPath
    setINISetting file "paths" "logPath" state.settings.logPath
    setINISetting file "paths" "publishPath" state.settings.publishPath
    setINISetting file "export" "preset" state.settings.exportPreset
    setINISetting file "export" "convertWithSwgTool" (state.settings.convertWithSwgTool as string)
    setINISetting file "export" "publishAfterExport" (state.settings.publishAfterExport as string)
)

fn swg_buildLogFile state assetName =
(
    swg_ensureDirectory state.settings.logPath
    local dotNetDateTime = dotNetClass "System.DateTime"
    local stamp = dotNetDateTime.Now.ToString "yyyyMMdd_HHmmss"
    state.logFile = state.settings.logPath + "/" + assetName + "_" + stamp + ".log"
    if SWG_MaxVersionInfo == undefined then SWG_MaxVersionInfo = swg_getMaxVersionInfo()
    swg_logMaxVersion state SWG_MaxVersionInfo
    state.logFile
)

fn swg_selectDirectory caption startPath =
(
    local folder = getSavePath caption caption startPath
    if folder == undefined then startPath else folder
)

fn swg_collectNodes includeHidden:off =
(
    local nodes = #()
    if selection.count > 0 then
    (
        for obj in selection do append nodes obj
    )
    else
    (
        for obj in geometry do if (includeHidden or obj.isHidden == false) do append nodes obj
    )
    nodes
)

fn swg_validateNaming nodes =
(
    local issues = #()
    for obj in nodes do
    (
        if matchPattern obj.name pattern:"*_LOD*" ignoreCase:true == false and
           matchPattern obj.name pattern:"hp_*" ignoreCase:true == false and
           matchPattern obj.name pattern:"col_*" ignoreCase:true == false then
        (
            append issues ("Node '" + obj.name + "' does not match SWG naming conventions.")
        )
    )
    issues
)

fn swg_validateTransforms nodes =
(
    local issues = #()
    for obj in nodes do
    (
        local scl = obj.scale
        if (length (scl - [1,1,1]) > 0.001) do append issues ("Node '" + obj.name + "' has non-uniform scale.")

        local rot = (obj.rotation as eulerAngles)
        if (abs rot.x > 0.1 or abs rot.y > 0.1 or abs rot.z > 0.1) do append issues ("Node '" + obj.name + "' has baked rotation; freeze transforms before export.")
    )
    issues
)

struct SWGMeshQualityCounts
(
    degenerateFaces = 0,
    unweldedVertices = 0,
    missingSmoothingFaces = 0,
    mixedNormalsFaces = 0,
    nonManifoldEdges = 0
)

struct SWGMeshQualityResult
(
    issues,
    counts
)

fn swg_validateMeshQuality nodes =
(
    local issues = #()
    local counts = SWGMeshQualityCounts()
    local areaEpsilon = 0.0001
    local weldKeyScale = 1000.0
    local unweldedThreshold = 5

    for obj in nodes do
    (
        if isKindOf obj geometryclass then
        (
            local mesh = undefined
            try(mesh = snapshotAsMesh obj) catch(mesh = undefined)
            if mesh != undefined then
            (
                local degenerateFaces = 0
                local missingSmoothing = 0
                local mixedNormals = 0
                local unweldedVertices = 0
                local nonManifoldEdges = 0

                for i = 1 to mesh.numfaces do
                (
                    local face = getFace mesh i
                    local v1 = getVert mesh face.x
                    local v2 = getVert mesh face.y
                    local v3 = getVert mesh face.z
                    local area = (length (cross (v2 - v1) (v3 - v1))) * 0.5
                    if area < areaEpsilon do degenerateFaces += 1

                    local smoothGroup = 0
                    try(smoothGroup = getFaceSmoothGroup mesh i) catch(smoothGroup = 0)
                    if smoothGroup == 0 do missingSmoothing += 1
                )

                local normalSum = [0,0,0]
                for i = 1 to mesh.numfaces do
                (
                    local faceNormal = [0,0,0]
                    try(faceNormal = meshop.getFaceNormal mesh i) catch(faceNormal = [0,0,0])
                    normalSum += faceNormal
                )
                if (length normalSum) > 0.0001 then
                (
                    local avgNormal = normalize normalSum
                    for i = 1 to mesh.numfaces do
                    (
                        local faceNormal = [0,0,0]
                        try(faceNormal = meshop.getFaceNormal mesh i) catch(faceNormal = [0,0,0])
                        if (dot (normalize faceNormal) avgNormal) < -0.25 do mixedNormals += 1
                    )
                )

                local vertsHash = dotNetObject "System.Collections.Hashtable"
                for i = 1 to mesh.numverts do
                (
                    local v = getVert mesh i
                    local key = ((round (v.x * weldKeyScale)) as string) + "_" + ((round (v.y * weldKeyScale)) as string) + "_" + ((round (v.z * weldKeyScale)) as string)
                    if vertsHash.ContainsKey key then
                    (
                        vertsHash.Item[key] = (vertsHash.Item[key]) + 1
                    )
                    else
                    (
                        vertsHash.Add key 1
                    )
                )
                for entry in vertsHash.Values do if entry > 1 do unweldedVertices += (entry - 1)

                try
                (
                    local edgeCount = meshop.getNumEdges mesh
                    for e = 1 to edgeCount do
                    (
                        local faces = meshop.getFacesUsingEdge mesh e
                        if faces.count > 2 do nonManifoldEdges += 1
                    )
                )
                catch()

                if degenerateFaces > 0 do
                (
                    append issues ("Mesh '" + obj.name + "' has " + (degenerateFaces as string) + " degenerate/zero-area face(s). How to fix: Run Prepare Geometry → Auto Fix Mesh Quality.")
                )
                if unweldedVertices > unweldedThreshold do
                (
                    append issues ("Mesh '" + obj.name + "' has " + (unweldedVertices as string) + " unwelded vertex overlap(s) (threshold " + (unweldedThreshold as string) + "). How to fix: Run Prepare Geometry → Auto Fix Mesh Quality.")
                )
                if missingSmoothing > 0 do
                (
                    append issues ("Mesh '" + obj.name + "' has " + (missingSmoothing as string) + " face(s) missing smoothing groups. How to fix: Apply smoothing groups or enable Auto Smooth, then Run Prepare Geometry → Auto Fix Mesh Quality.")
                )
                if mixedNormals > 0 do
                (
                    append issues ("Mesh '" + obj.name + "' has " + (mixedNormals as string) + " face(s) with mixed normals. How to fix: Flip incorrect normals and Run Prepare Geometry → Auto Fix Mesh Quality.")
                )
                if nonManifoldEdges > 0 do
                (
                    append issues ("Mesh '" + obj.name + "' has " + (nonManifoldEdges as string) + " non-manifold edge(s). How to fix: Run Prepare Geometry → Auto Fix Mesh Quality.")
                )

                counts.degenerateFaces += degenerateFaces
                counts.unweldedVertices += unweldedVertices
                counts.missingSmoothingFaces += missingSmoothing
                counts.mixedNormalsFaces += mixedNormals
                counts.nonManifoldEdges += nonManifoldEdges
            )
        )
    )

    SWGMeshQualityResult (issues:issues, counts:counts)
)

fn swg_buildValidationSummary namingCount transformCount meshIssueCount meshCounts totalIssues =
(
    local summary = ""
    local meshDetail = "Mesh quality: " + (meshIssueCount as string) + " issue(s)"
    meshDetail += " (degenerate " + (meshCounts.degenerateFaces as string)
    meshDetail += ", unwelded " + (meshCounts.unweldedVertices as string)
    meshDetail += ", smoothing " + (meshCounts.missingSmoothingFaces as string)
    meshDetail += ", mixed normals " + (meshCounts.mixedNormalsFaces as string)
    meshDetail += ", non-manifold " + (meshCounts.nonManifoldEdges as string) + ")."

    if totalIssues == 0 then
    (
        summary = "Last validation: Passed. Naming: " + (namingCount as string) + ", Transform: " + (transformCount as string) + ". " + meshDetail
    )
    else
    (
        summary = "Last validation: " + (totalIssues as string) + " issue(s). Naming: " + (namingCount as string) + ", Transform: " + (transformCount as string) + ". " + meshDetail
    )
    summary
)

fn swg_validateScene state =
(
    local nodes = swg_collectNodes includeHidden:on
    local issues = #()
    local namingIssues = swg_validateNaming nodes
    local transformIssues = swg_validateTransforms nodes
    local meshQualityResult = swg_validateMeshQuality nodes
    local meshQualityIssues = meshQualityResult.issues
    local meshQualityCounts = meshQualityResult.counts

    for issue in namingIssues do
    (
        append issues (issue + " How to fix: Rename nodes to *_LOD#, hp_*, or col_*.")
    )

    for issue in transformIssues do
    (
        local fix = "How to fix: Reset transforms before export."
        if (findString issue "non-uniform scale") != undefined do fix = "How to fix: Reset XForm to 100% scale, then collapse."
        if (findString issue "baked rotation") != undefined do fix = "How to fix: Reset XForm or Freeze Transforms to zero rotation."
        append issues (issue + " " + fix)
    )

    for issue in meshQualityIssues do
    (
        append issues issue
    )

    local summary = swg_buildValidationSummary namingIssues.count transformIssues.count meshQualityIssues.count meshQualityCounts issues.count

    if issues.count == 0 then
    (
        swg_log state "Scene validation passed."
        state.validationSummary = summary
        if state.rollout != undefined do try(state.rollout.txtValidationSummary.text = state.validationSummary)catch()
        messageBox "Validation complete. No issues found." title:"SWG Exporter"
        true
    )
    else
    (
        local message = "Validation completed with warnings:\n\n"
        for issue in issues do message += issue + "\n"
        swg_log state message
        state.validationSummary = summary
        if state.rollout != undefined do try(state.rollout.txtValidationSummary.text = state.validationSummary)catch()
        messageBox message title:"SWG Exporter" beep:true
        false
    )
)

fn swg_exportSelection state preset filePath =
(
    if not (swg_preflight state) do return false
    if selection.count == 0 do select (swg_collectNodes())
    if selection.count == 0 then
    (
        messageBox "No objects available to export." title:"SWG Exporter"
        return false
    )

    try
    (
        exportFile filePath #noPrompt selectedOnly:true using:FBXEXP
        true
    )
    catch
    (
        messageBox ("Export failed. Check that the FBX exporter is installed.\n" + getCurrentException()) title:"SWG Exporter"
        false
    )
)

fn swg_isFbxExporterAvailable =
(
    local available = false
    try
    (
        if (classOf FBXEXP) != undefined do available = true
    )
    catch()

    if not available do
    (
        try
        (
            local classes = exporterPluginClasses()
            for c in classes while not available do
            (
                local className = ""
                try(className = (classOf c) as string) catch(className = "")
                if className == "FBXEXP" do available = true
            )
        )
        catch()
    )

    available
)

fn swg_canResolvePython pythonExe =
(
    local proc = dotNetObject "System.Diagnostics.Process"
    proc.StartInfo.FileName = pythonExe
    proc.StartInfo.Arguments = "--version"
    proc.StartInfo.UseShellExecute = false
    proc.StartInfo.RedirectStandardOutput = true
    proc.StartInfo.RedirectStandardError = true
    proc.StartInfo.CreateNoWindow = true

    local started = false
    try(started = proc.Start()) catch(started = false)
    if not started do return false
    proc.WaitForExit()
    proc.ExitCode == 0
)

fn swg_preflight state =
(
    local issues = #()
    local ok = true

    local fbxAvailable = swg_isFbxExporterAvailable()
    swg_log state ("Preflight: FBX exporter " + (if fbxAvailable then "available." else "not available."))
    if not fbxAvailable do
    (
        append issues "FBX exporter not found. Install the Autodesk FBX exporter or enable the FBX plugin."
        ok = false
    )

    local pythonExe = getEnv "SWG_PYTHON"
    if pythonExe == undefined or pythonExe == "" do pythonExe = "python"
    local pythonAvailable = swg_canResolvePython pythonExe
    swg_log state ("Preflight: Python command '" + pythonExe + "' " + (if pythonAvailable then "resolved." else "not found." ))
    if not pythonAvailable do
    (
        append issues ("Python not found. Set SWG_PYTHON env var to your Python executable or ensure 'python' is on PATH.")
        ok = false
    )

    local swgTool = state.settings.projectRoot + "/tools/swg-tool.py"
    local swgToolExists = doesFileExist swgTool
    swg_log state ("Preflight: swg-tool.py " + (if swgToolExists then "located at " + swgTool + "." else "missing at " + swgTool + "." ))
    if not swgToolExists do
    (
        append issues "swg-tool.py missing. Verify tools/swg-tool.py under Project Root."
        ok = false
    )

    if not ok then
    (
        local message = "Export preflight failed:\n\n"
        for issue in issues do message += ("• " + issue + "\n")
        messageBox message title:"SWG Exporter"
    )

    ok
)

fn swg_runSwgTool state exportType sourceFile targetFile =
(
    if not (swg_preflight state) do return false
    local pythonExe = getEnv "SWG_PYTHON"
    if pythonExe == undefined or pythonExe == "" do pythonExe = "python"

    local swgTool = state.settings.projectRoot + "/tools/swg-tool.py"
    if not (doesFileExist swgTool) do return false

    local args = ("\"" + swgTool + "\" export --type " + exportType + " --input \"" + sourceFile + "\" --output \"" + targetFile + "\"")
    local command = pythonExe

    local proc = dotNetObject "System.Diagnostics.Process"
    proc.StartInfo.FileName = command
    proc.StartInfo.Arguments = args
    proc.StartInfo.WorkingDirectory = state.settings.projectRoot
    proc.StartInfo.UseShellExecute = false
    proc.StartInfo.RedirectStandardOutput = true
    proc.StartInfo.RedirectStandardError = true
    proc.StartInfo.CreateNoWindow = true
    proc.Start()
    proc.WaitForExit()

    local output = proc.StandardOutput.ReadToEnd()
    local errorOutput = proc.StandardError.ReadToEnd()
    if output != "" do swg_log state output
    if errorOutput != "" do swg_log state ("ERROR: " + errorOutput)

    proc.ExitCode == 0
)

fn swg_publishAssets state files =
(
    if files.count == 0 do return true

    local changelistFile = state.settings.publishPath + "/swg_exporter_cl.txt"
    swg_ensureDirectory state.settings.publishPath
    local stream = createFile changelistFile
    for file in files do format "%\n" file to:stream
    close stream
    swg_log state ("Wrote changelist file: " + changelistFile)
    true
)

fn swg_performExport state exportType =
(
    if state.mutex then
    (
        messageBox "An export is already in progress." title:"SWG Exporter"
        return false
    )

    state.mutex = true

    local assetName = (if selection.count > 0 then selection[1].name else (maxFileName as string))
    if assetName == undefined or assetName == "" do assetName = "swg_asset"
    local tempDir = getDir #temp + "/swg_exporter"
    swg_ensureDirectory tempDir

    local exportDirectory = case exportType of
    (
        #static: state.settings.meshPath
        #skeletal: state.settings.skeletonPath
        #animation: state.settings.animationPath
        #portal: state.settings.appearancePath
        #collision: state.settings.collisionPath
        default: state.settings.meshPath
    )
    swg_ensureDirectory exportDirectory

    local tempFile = tempDir + "/" + assetName + "_" + (exportType as string) + ".fbx"
    local finalExtension = case exportType of
    (
        #static: ".msh"
        #skeletal: ".skm"
        #animation: ".ska"
        #portal: ".cmp"
        #collision: ".clm"
        default: ".msh"
    )
    local finalFile = exportDirectory + "/" + assetName + finalExtension

    swg_buildLogFile state assetName
    swg_log state ("Export started for " + assetName + " as " + (exportType as string))

    if swg_exportSelection state state.settings.exportPreset tempFile then
    (
        local exportedFiles = #()
        if state.settings.convertWithSwgTool then
        (
            if swg_runSwgTool state (exportType as string) tempFile finalFile then
            (
                append exportedFiles finalFile
                swg_log state ("SWG conversion finished: " + finalFile)
                if doesFileExist tempFile do deleteFile tempFile
            )
            else
            (
                append exportedFiles tempFile
                swg_log state "Conversion failed; leaving FBX file for manual inspection."
            )
        )
        else
        (
            if doesFileExist finalFile do deleteFile finalFile
            dotNetClass "System.IO.File".Copy tempFile finalFile true
            if doesFileExist tempFile do deleteFile tempFile
            append exportedFiles finalFile
        )

        if state.settings.publishAfterExport then swg_publishAssets state exportedFiles
        state.mutex = false
        local summary = ""
        for f in exportedFiles do summary += f + "\n"
        messageBox ("Export completed:\n" + summary) title:"SWG Exporter"
        true
    )
    else
    (
        state.mutex = false
        false
    )
)

fn swg_updateRolloutFromSettings ro state =
(
    ro.edtProjectRoot.text = state.settings.projectRoot
    ro.edtAppearance.text = state.settings.appearancePath
    ro.edtMesh.text = state.settings.meshPath
    ro.edtSkeleton.text = state.settings.skeletonPath
    ro.edtAnimation.text = state.settings.animationPath
    ro.edtShader.text = state.settings.shaderPath
    ro.edtCollision.text = state.settings.collisionPath
    ro.edtLog.text = state.settings.logPath
    ro.edtPublish.text = state.settings.publishPath
    ro.chkConvert.checked = state.settings.convertWithSwgTool
    ro.chkPublish.checked = state.settings.publishAfterExport
    local idx = findItem ro.cmbPreset.items state.settings.exportPreset
    if idx == 0 do idx = 1
    ro.cmbPreset.selection = idx
    if ro.txtValidationSummary != undefined do ro.txtValidationSummary.text = state.validationSummary
)

fn swg_applyRolloutToSettings ro state =
(
    state.settings.projectRoot = ro.edtProjectRoot.text
    state.settings.appearancePath = ro.edtAppearance.text
    state.settings.meshPath = ro.edtMesh.text
    state.settings.skeletonPath = ro.edtSkeleton.text
    state.settings.animationPath = ro.edtAnimation.text
    state.settings.shaderPath = ro.edtShader.text
    state.settings.collisionPath = ro.edtCollision.text
    state.settings.logPath = ro.edtLog.text
    state.settings.publishPath = ro.edtPublish.text
    state.settings.convertWithSwgTool = ro.chkConvert.checked
    state.settings.publishAfterExport = ro.chkPublish.checked
    if ro.cmbPreset.selection > 0 do state.settings.exportPreset = ro.cmbPreset.items[ro.cmbPreset.selection]
    swg_saveSettings state
)

if SWGExporter != undefined do
(
    if SWGExporter.rollout != undefined do try(destroyDialog SWGExporter.rollout)catch()
    SWGExporter.rollout = undefined
)

fn swg_createRollout =
(
    rollout ro "SWG Exporter" width:400
    (
        fn swg_getState =
        (
            if SWGExporter == undefined then
            (
                messageBox "SWG Exporter is not initialized. Please run swg_install()." title:"SWG Exporter"
                undefined
            )
            else
            (
                SWGExporter
            )
        )

        fn swg_beforeAction =
        (
            local state = swg_getState()
            if state != undefined do swg_applyRolloutToSettings ro state
            state
        )

        group "Project"
        (
            edittext edtProjectRoot "Project Root:" text:"" fieldWidth:280 tooltip:"Root folder containing tools/ and asset output directories."
            button btnBrowseProject "Browse" width:80 tooltip:"Choose the SWG project root folder."
        )
        group "Output Paths"
        (
            edittext edtAppearance "Appearance:" text:"" fieldWidth:280 tooltip:"Output folder for .cmp appearance files (e.g., <project>/appearance)."
            button btnBrowseAppearance "Browse" width:80 tooltip:"Pick the appearance output directory."

            edittext edtMesh "Meshes:" text:"" fieldWidth:280 tooltip:"Output folder for static meshes (.msh)."
            button btnBrowseMesh "Browse" width:80 tooltip:"Pick the mesh output directory."

            edittext edtSkeleton "Skeletons:" text:"" fieldWidth:280 tooltip:"Output folder for skeletal meshes (.skm)."
            button btnBrowseSkeleton "Browse" width:80 tooltip:"Pick the skeleton output directory."

            edittext edtAnimation "Animations:" text:"" fieldWidth:280 tooltip:"Output folder for animations (.ska)."
            button btnBrowseAnimation "Browse" width:80 tooltip:"Pick the animation output directory."

            edittext edtShader "Shaders:" text:"" fieldWidth:280 tooltip:"Output folder for shader definitions."
            button btnBrowseShader "Browse" width:80 tooltip:"Pick the shader output directory."

            edittext edtCollision "Collision:" text:"" fieldWidth:280 tooltip:"Output folder for collision meshes (.clm)."
            button btnBrowseCollision "Browse" width:80 tooltip:"Pick the collision output directory."

            edittext edtLog "Logs:" text:"" fieldWidth:280 tooltip:"Folder where exporter logs are written."
            button btnBrowseLog "Browse" width:80 tooltip:"Pick the log output directory."

            edittext edtPublish "Publish:" text:"" fieldWidth:280 tooltip:"Folder used for publish changelists."
            button btnBrowsePublish "Browse" width:80 tooltip:"Pick the publish output directory."
        )

        group "Export"
        (
            dropdownlist cmbPreset "FBX Preset" items:#("GameExporter", "Default", "Previous")
            checkbox chkConvert "Convert with swg-tool" checked:true tooltip:"Run tools/swg-tool.py after FBX export."
            checkbox chkPublish "Publish after export" checked:false tooltip:"Write a changelist file to the Publish folder."

            button btnValidate "Validate Scene" width:180 tooltip:"Check naming and transforms before export."
            button btnExportStatic "Export Static Mesh" width:180 tooltip:"Export selected geometry to .msh."
            button btnExportSkeletal "Export Skeletal Mesh" width:180 tooltip:"Export skeletal meshes to .skm."
            button btnExportAnimation "Export Animation" width:180 tooltip:"Export animation data to .ska."
            button btnExportPortal "Export Portal/Cell" width:180 tooltip:"Export portal/cell data to .cmp."
            button btnExportCollision "Export Collision" width:180 tooltip:"Export collision meshes to .clm."
            button btnHelp "Help" width:180 tooltip:"Show the export checklist."

            staticText txtValidationSummary "Last validation: Not run." width:360
        )

        on ro open do
        (
            local state = swg_getState()
            if state != undefined do swg_updateRolloutFromSettings ro state
        )

        on btnBrowseProject pressed do
        (
            local state = swg_getState()
            if state != undefined do ro.edtProjectRoot.text = swg_selectDirectory "Project Root" state.settings.projectRoot
        )
        on btnBrowseAppearance pressed do
        (
            local state = swg_getState()
            if state != undefined do ro.edtAppearance.text = swg_selectDirectory "Appearance" state.settings.appearancePath
        )
        on btnBrowseMesh pressed do
        (
            local state = swg_getState()
            if state != undefined do ro.edtMesh.text = swg_selectDirectory "Meshes" state.settings.meshPath
        )
        on btnBrowseSkeleton pressed do
        (
            local state = swg_getState()
            if state != undefined do ro.edtSkeleton.text = swg_selectDirectory "Skeletons" state.settings.skeletonPath
        )
        on btnBrowseAnimation pressed do
        (
            local state = swg_getState()
            if state != undefined do ro.edtAnimation.text = swg_selectDirectory "Animations" state.settings.animationPath
        )
        on btnBrowseShader pressed do
        (
            local state = swg_getState()
            if state != undefined do ro.edtShader.text = swg_selectDirectory "Shader" state.settings.shaderPath
        )
        on btnBrowseCollision pressed do
        (
            local state = swg_getState()
            if state != undefined do ro.edtCollision.text = swg_selectDirectory "Collision" state.settings.collisionPath
        )
        on btnBrowseLog pressed do
        (
            local state = swg_getState()
            if state != undefined do ro.edtLog.text = swg_selectDirectory "Logs" state.settings.logPath
        )
        on btnBrowsePublish pressed do
        (
            local state = swg_getState()
            if state != undefined do ro.edtPublish.text = swg_selectDirectory "Publish" state.settings.publishPath
        )

        on btnValidate pressed do
        (
            local state = swg_beforeAction()
            if state != undefined do swg_validateScene state
        )

        on btnExportStatic pressed do
        (
            local state = swg_beforeAction()
            if state != undefined do swg_performExport state #static
        )

        on btnExportSkeletal pressed do
        (
            local state = swg_beforeAction()
            if state != undefined do swg_performExport state #skeletal
        )

        on btnExportAnimation pressed do
        (
            local state = swg_beforeAction()
            if state != undefined do swg_performExport state #animation
        )

        on btnExportPortal pressed do
        (
            local state = swg_beforeAction()
            if state != undefined do swg_performExport state #portal
        )

        on btnExportCollision pressed do
        (
            local state = swg_beforeAction()
            if state != undefined do swg_performExport state #collision
        )

        on btnHelp pressed do
        (
            messageBox ("SWG Export Checklist:\n" +
                        "• Naming: use *_LOD#, hp_*, col_*.\n" +
                        "• Transforms: reset scale/rotation before export.\n" +
                        "• Export preset: choose the correct FBX preset.\n" +
                        "• swg-tool path: tools/swg-tool.py exists under Project Root.") title:"SWG Exporter Help"
        )
    )
    ro
)

fn swg_showExporterDialog =
(
    if not swg_install() do return false
    if SWGExporter.rollout == undefined do SWGExporter.rollout = swg_createRollout()
    if SWGExporter.rollout != undefined do swg_updateRolloutFromSettings SWGExporter.rollout SWGExporter
    if SWGExporter.rollout != undefined do try(destroyDialog SWGExporter.rollout)catch()
    if SWGExporter.rollout != undefined do createDialog SWGExporter.rollout style:#(#style_titlebar, #style_sysmenu, #style_toolwindow)
)

fn swg_install =
(
    if not swg_checkMaxVersion() do return false
    local firstInstall = false
    if SWGExporter == undefined then
    (
        SWGExporter = SWGExporterState()
        SWGExporter.settingsFile = swg_getSettingsFile()
        swg_ensureDirectory (getFilenamePath SWGExporter.settingsFile)
        swg_loadSettings SWGExporter
        firstInstall = true
    )
    if SWGExporter.rollout == undefined do SWGExporter.rollout = swg_createRollout()
    if firstInstall do format "[SWG Exporter] Installed.\n"
    true
)

fn swg_uninstall =
(
    if SWGExporter != undefined do
    (
        if SWGExporter.rollout != undefined do try(destroyDialog SWGExporter.rollout)catch()
        SWGExporter.rollout = undefined
        SWGExporter = undefined
    )
    if macros.isRegistered "Star Wars Galaxies" "SWGExporter" do macros.delete "Star Wars Galaxies" "SWGExporter"
    format "[SWG Exporter] Uninstalled.\n"
    true
)

if macros.isRegistered "Star Wars Galaxies" "SWGExporter" do macros.delete "Star Wars Galaxies" "SWGExporter"

macroScript SWGExporter category:"Star Wars Galaxies" tooltip:"SWG Exporter"
(
    on execute do swg_showExporterDialog()
)

if swg_checkMaxVersion() do swg_install()
