#include "MainWindow.h"

#include "../swg_creation_tool/IffBuilder.h"
#include "../swg_creation_tool/Json.h"

#include <QAbstractItemView>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QSplitter>
#include <QVBoxLayout>

namespace {
QStringList treFilters() {
    return {"Tree archives (*.tre *.tres)", "All files (*)"};
}

QString defaultDefinition() {
    return R"JSON({
  "form": "TEST",
  "children": [
    {"chunk": "INFO", "data": "SWG tool suite"},
    {"chunk": "VERS", "data": 1},
    {"form": "DATA", "children": [
      {"chunk": "TEXT", "data": "aGVsbG8=", "encoding": "base64"}
    ]}
  ]
})JSON";
}
} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildArchivePane(), tr("Archives"));
    m_tabs->addTab(buildCreationPane(), tr("IFF Builder"));
    m_tabs->addTab(buildEncryptionPane(), tr("TRES Encryption"));

    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(m_tabs);
    central->setLayout(layout);

    setWindowTitle(tr("SWG Tool Studio"));
    resize(1200, 720);

    refreshList();
    refreshIffPreview();
}

QWidget *MainWindow::buildArchivePane() {
    QWidget *panel = new QWidget(this);

    m_entryList = new QListWidget(this);
    m_entryList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_entryList, &QListWidget::itemSelectionChanged, this, &MainWindow::selectionChanged);

    m_preview = new QPlainTextEdit(this);
    m_preview->setReadOnly(true);
    m_preview->setPlaceholderText("Select an entry to see its contents");

    m_openButton = new QPushButton(tr("Open"), this);
    m_saveButton = new QPushButton(tr("Save"), this);
    m_addButton = new QPushButton(tr("Add Files"), this);
    m_removeButton = new QPushButton(tr("Remove"), this);
    auto *newButton = new QPushButton(tr("New"), this);

    connect(m_openButton, &QPushButton::clicked, this, &MainWindow::openArchive);
    connect(m_saveButton, &QPushButton::clicked, this, &MainWindow::saveArchive);
    connect(m_addButton, &QPushButton::clicked, this, &MainWindow::addFiles);
    connect(m_removeButton, &QPushButton::clicked, this, &MainWindow::removeSelection);
    connect(newButton, &QPushButton::clicked, this, &MainWindow::newArchive);

    QHBoxLayout *buttonRow = new QHBoxLayout();
    buttonRow->addWidget(newButton);
    buttonRow->addWidget(m_openButton);
    buttonRow->addWidget(m_saveButton);
    buttonRow->addStretch();
    buttonRow->addWidget(m_addButton);
    buttonRow->addWidget(m_removeButton);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_entryList);
    splitter->addWidget(m_preview);
    splitter->setStretchFactor(1, 1);

    QVBoxLayout *layout = new QVBoxLayout();
    layout->addLayout(buttonRow);
    layout->addWidget(splitter);
    panel->setLayout(layout);
    return panel;
}

QWidget *MainWindow::buildCreationPane() {
    QWidget *panel = new QWidget(this);

    m_definitionEdit = new QPlainTextEdit(this);
    m_definitionEdit->setPlaceholderText(tr("Paste an IFF JSON definition (forms and chunks)"));
    m_definitionEdit->setPlainText(defaultDefinition());
    connect(m_definitionEdit, &QPlainTextEdit::textChanged, this, &MainWindow::refreshIffPreview);

    m_treePreview = new QPlainTextEdit(this);
    m_treePreview->setReadOnly(true);
    m_treePreview->setPlaceholderText(tr("Structure preview"));

    m_iffPreview = new QPlainTextEdit(this);
    m_iffPreview->setReadOnly(true);
    m_iffPreview->setPlaceholderText(tr("Binary preview"));

    auto *definitionButtons = new QHBoxLayout();
    auto *loadButton = new QPushButton(tr("Load Definition"), this);
    auto *saveButton = new QPushButton(tr("Save Definition"), this);
    m_exportButton = new QPushButton(tr("Save IFF"), this);
    m_attachButton = new QPushButton(tr("Add to Archive"), this);
    m_storeUncompressed = new QCheckBox(tr("Store raw bytes (skip zlib)"), this);
    m_storeUncompressed->setChecked(false);

    m_entryName = new QLineEdit(tr("new_file.iff"), this);
    m_definitionStatus = new QLabel(tr("Waiting for definition..."), this);

    connect(loadButton, &QPushButton::clicked, this, &MainWindow::loadDefinition);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveDefinition);
    connect(m_exportButton, &QPushButton::clicked, this, &MainWindow::exportIff);
    connect(m_attachButton, &QPushButton::clicked, this, &MainWindow::attachIffToArchive);

    definitionButtons->addWidget(loadButton);
    definitionButtons->addWidget(saveButton);
    definitionButtons->addWidget(m_exportButton);
    definitionButtons->addStretch();
    definitionButtons->addWidget(new QLabel(tr("Archive name:"), this));
    definitionButtons->addWidget(m_entryName);
    definitionButtons->addWidget(m_storeUncompressed);
    definitionButtons->addWidget(m_attachButton);

    QSplitter *previewSplitter = new QSplitter(Qt::Horizontal, this);
    previewSplitter->addWidget(m_treePreview);
    previewSplitter->addWidget(m_iffPreview);
    previewSplitter->setStretchFactor(0, 1);
    previewSplitter->setStretchFactor(1, 1);

    QVBoxLayout *layout = new QVBoxLayout();
    layout->addLayout(definitionButtons);
    layout->addWidget(m_definitionStatus);
    layout->addWidget(m_definitionEdit);
    layout->addWidget(previewSplitter);
    panel->setLayout(layout);
    return panel;
}

QWidget *MainWindow::buildEncryptionPane() {
    QWidget *panel = new QWidget(this);

    m_encryptInputPath = new QLineEdit(this);
    m_encryptInputPath->setPlaceholderText(tr("Select a source .tre or .tres file"));
    auto *inputBrowse = new QPushButton(tr("Browse"), this);

    m_encryptOutputPath = new QLineEdit(this);
    m_encryptOutputPath->setPlaceholderText(tr("Select an output .tres file"));
    auto *outputBrowse = new QPushButton(tr("Browse"), this);

    m_encryptPassphrase = new QLineEdit(this);
    m_encryptPassphrase->setEchoMode(QLineEdit::Password);
    m_encryptPassphrase->setPlaceholderText(tr("Passphrase for encrypted .tres"));

    m_encryptButton = new QPushButton(tr("Create Encrypted TRES"), this);
    m_encryptStatus = new QLabel(tr("Choose input, output, and passphrase to encrypt."), this);
    m_encryptStatus->setStyleSheet("color: #666;");

    auto *inputRow = new QHBoxLayout();
    inputRow->addWidget(new QLabel(tr("Source archive:"), this));
    inputRow->addWidget(m_encryptInputPath);
    inputRow->addWidget(inputBrowse);

    auto *outputRow = new QHBoxLayout();
    outputRow->addWidget(new QLabel(tr("Output archive:"), this));
    outputRow->addWidget(m_encryptOutputPath);
    outputRow->addWidget(outputBrowse);

    auto *passphraseRow = new QHBoxLayout();
    passphraseRow->addWidget(new QLabel(tr("Passphrase:"), this));
    passphraseRow->addWidget(m_encryptPassphrase);

    auto *layout = new QVBoxLayout();
    layout->addLayout(inputRow);
    layout->addLayout(outputRow);
    layout->addLayout(passphraseRow);
    layout->addWidget(m_encryptButton);
    layout->addWidget(m_encryptStatus);
    layout->addStretch();
    panel->setLayout(layout);

    connect(inputBrowse, &QPushButton::clicked, this, [this]() {
        QFileDialog dialog(this, tr("Select source archive"));
        dialog.setFileMode(QFileDialog::ExistingFile);
        dialog.setNameFilters(treFilters());
        if (!dialog.exec()) {
            return;
        }
        const auto selection = dialog.selectedFiles();
        if (!selection.isEmpty()) {
            m_encryptInputPath->setText(selection.first());
        }
    });

    connect(outputBrowse, &QPushButton::clicked, this, [this]() {
        QFileDialog dialog(this, tr("Select output archive"));
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setNameFilters({tr("Encrypted tree archive (*.tres)"), tr("All files (*)")});
        if (!dialog.exec()) {
            return;
        }
        const auto selection = dialog.selectedFiles();
        if (!selection.isEmpty()) {
            m_encryptOutputPath->setText(selection.first());
        }
    });

    connect(m_encryptButton, &QPushButton::clicked, this, [this]() {
        const QString inputPath = m_encryptInputPath->text().trimmed();
        const QString outputPath = m_encryptOutputPath->text().trimmed();
        const QString passphrase = m_encryptPassphrase->text();

        if (inputPath.isEmpty() || outputPath.isEmpty()) {
            QMessageBox::warning(this, tr("Missing paths"), tr("Select both input and output paths."));
            return;
        }
        if (passphrase.trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Missing passphrase"), tr("Enter a passphrase to encrypt the archive."));
            return;
        }

        const QFileInfo inputInfo(inputPath);
        if (!inputInfo.exists()) {
            QMessageBox::warning(this, tr("Missing input"), tr("The selected input file does not exist."));
            return;
        }

        const QFileInfo outputInfo(outputPath);
        if (outputInfo.suffix().compare("tres", Qt::CaseInsensitive) != 0) {
            const auto choice = QMessageBox::question(this, tr("Output extension"),
                                                      tr("Output does not end with .tres. Continue anyway?"));
            if (choice != QMessageBox::Yes) {
                return;
            }
        }

        try {
            const std::string passphraseText = passphrase.toStdString();
            const bool inputEncrypted = inputInfo.suffix().compare("tres", Qt::CaseInsensitive) == 0;
            TreArchive archive = TreArchive::load(inputPath.toStdString(), inputEncrypted ? passphraseText : std::string());
            archive.save(outputPath.toStdString(), passphraseText);
            m_encryptStatus->setText(tr("Encrypted archive saved to %1").arg(outputPath));
            m_encryptStatus->setStyleSheet("color: #0b7300;");
        } catch (const std::exception &err) {
            m_encryptStatus->setText(tr("Failed: %1").arg(QString::fromUtf8(err.what())));
            m_encryptStatus->setStyleSheet("color: #a00000;");
            QMessageBox::critical(this, tr("Encryption failed"), QString::fromUtf8(err.what()));
        }
    });

    return panel;
}

void MainWindow::newArchive() {
    m_archive = TreArchive();
    refreshList();
    statusBar()->showMessage(tr("Started a new archive"), 2000);
}

void MainWindow::openArchive() {
    const QString path = promptForArchivePath(false);
    if (path.isEmpty()) {
        return;
    }
    const QFileInfo fileInfo(path);
    std::string passphrase;
    if (fileInfo.suffix().compare("tres", Qt::CaseInsensitive) == 0) {
        const QString prompt = promptForPassphrase(tr("Open encrypted archive"), tr("Enter passphrase to decrypt .tres"));
        if (prompt.isEmpty()) {
            return;
        }
        passphrase = prompt.toStdString();
    }
    try {
        m_archive = TreArchive::load(path.toStdString(), passphrase);
        refreshList();
        statusBar()->showMessage(tr("Loaded %1").arg(path), 3000);
        m_tabs->setCurrentIndex(0);
    } catch (const std::exception &err) {
        QMessageBox::critical(this, tr("Failed to open"), QString::fromUtf8(err.what()));
    }
}

void MainWindow::saveArchive() {
    const QString path = promptForArchivePath(true);
    if (path.isEmpty()) {
        return;
    }
    const QFileInfo fileInfo(path);
    std::string passphrase;
    if (fileInfo.suffix().compare("tres", Qt::CaseInsensitive) == 0) {
        const QString prompt = promptForPassphrase(tr("Encrypt archive"), tr("Choose a passphrase for the .tres file"));
        if (prompt.isEmpty()) {
            return;
        }
        passphrase = prompt.toStdString();
    }
    try {
        m_archive.save(path.toStdString(), passphrase);
        statusBar()->showMessage(tr("Saved to %1").arg(path), 3000);
    } catch (const std::exception &err) {
        QMessageBox::critical(this, tr("Failed to save"), QString::fromUtf8(err.what()));
    }
}

void MainWindow::addFiles() {
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setNameFilters({tr("All files (*)")});
    if (!dialog.exec()) {
        return;
    }
    const auto files = dialog.selectedFiles();
    for (const QString &file : files) {
        const QFileInfo fileInfo(file);
        const std::string entryName = fileInfo.fileName().toStdString();
        try {
            m_archive.add_file(file.toStdString(), entryName);
        } catch (const std::exception &err) {
            QMessageBox::critical(this, tr("Failed to add file"), QString::fromUtf8(err.what()));
            break;
        }
    }
    refreshList();
}

void MainWindow::removeSelection() {
    const auto selected = m_entryList->currentRow();
    if (selected < 0) {
        return;
    }
    m_archive.remove_entry(static_cast<std::size_t>(selected));
    refreshList();
}

void MainWindow::selectionChanged() {
    updatePreview();
}

void MainWindow::refreshIffPreview() {
    const QString text = m_definitionEdit->toPlainText();
    const std::string compact = text.trimmed().toStdString();
    if (compact.empty()) {
        m_definitionStatus->setText(tr("Waiting for definition..."));
        m_definitionStatus->setStyleSheet("color: #666;");
        m_treePreview->clear();
        m_iffPreview->clear();
        m_lastIff.clear();
        m_exportButton->setEnabled(false);
        m_attachButton->setEnabled(false);
        return;
    }

    try {
        const JsonValue root = parse_json(text.toStdString());
        const IffBuilder builder = IffBuilder::from_definition(root);
        m_lastIff = builder.build_bytes();
        const std::string describe = builder.describe();

        m_treePreview->setPlainText(QString::fromStdString(describe));

        QString content = QString::fromStdString(format_bytes(m_lastIff));
        content.append("\n\nPreview as text:\n");
        content.append(QString::fromUtf8(reinterpret_cast<const char *>(m_lastIff.data()),
                                         static_cast<int>(m_lastIff.size())));
        m_iffPreview->setPlainText(content);

        m_definitionStatus->setText(tr("Definition valid • %1 bytes ready").arg(
            static_cast<qulonglong>(m_lastIff.size())));
        m_definitionStatus->setStyleSheet("color: #0b7300;");
        m_exportButton->setEnabled(true);
        m_attachButton->setEnabled(true);
    } catch (const std::exception &err) {
        m_lastIff.clear();
        m_treePreview->clear();
        m_iffPreview->setPlainText(QString::fromUtf8(err.what()));
        m_definitionStatus->setText(tr("Invalid definition: %1").arg(QString::fromUtf8(err.what())));
        m_definitionStatus->setStyleSheet("color: #a00000;");
        m_exportButton->setEnabled(false);
        m_attachButton->setEnabled(false);
    }
}

void MainWindow::loadDefinition() {
    QFileDialog dialog(this, tr("Open definition"));
    dialog.setNameFilters({tr("JSON (*.json)")});
    dialog.setFileMode(QFileDialog::ExistingFile);
    if (!dialog.exec()) {
        return;
    }
    const auto selection = dialog.selectedFiles();
    if (selection.isEmpty()) {
        return;
    }
    QFile file(selection.first());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Failed to open"), file.errorString());
        return;
    }
    const QString content = QString::fromUtf8(file.readAll());
    m_definitionEdit->setPlainText(content);
    refreshIffPreview();
}

void MainWindow::saveDefinition() {
    QFileDialog dialog(this, tr("Save definition"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilters({tr("JSON (*.json)"), tr("All files (*)")});
    if (!dialog.exec()) {
        return;
    }
    const auto selection = dialog.selectedFiles();
    if (selection.isEmpty()) {
        return;
    }
    QFile file(selection.first());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Failed to save"), file.errorString());
        return;
    }
    file.write(m_definitionEdit->toPlainText().toUtf8());
}

void MainWindow::exportIff() {
    if (m_lastIff.empty()) {
        refreshIffPreview();
    }
    if (m_lastIff.empty()) {
        QMessageBox::warning(this, tr("No IFF"), tr("Provide a valid definition before saving."));
        return;
    }

    QFileDialog dialog(this, tr("Save IFF"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilters({tr("IFF (*.iff)"), tr("All files (*)")});
    if (!dialog.exec()) {
        return;
    }
    const auto selection = dialog.selectedFiles();
    if (selection.isEmpty()) {
        return;
    }
    QFile file(selection.first());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, tr("Failed to save"), file.errorString());
        return;
    }
    file.write(reinterpret_cast<const char *>(m_lastIff.data()), static_cast<qint64>(m_lastIff.size()));
    statusBar()->showMessage(tr("Saved IFF to %1").arg(selection.first()), 3000);
}

void MainWindow::attachIffToArchive() {
    if (m_lastIff.empty()) {
        QMessageBox::warning(this, tr("No IFF"), tr("Provide a valid definition before attaching."));
        return;
    }
    const QString nameText = m_entryName->text().trimmed();
    if (nameText.isEmpty()) {
        QMessageBox::warning(this, tr("Missing name"), tr("Enter an archive entry name for the IFF payload."));
        return;
    }
    try {
        m_archive.add_bytes(nameText.toStdString(), m_lastIff, m_storeUncompressed->isChecked());
        refreshList();
        statusBar()->showMessage(tr("Added %1 to archive").arg(nameText), 3000);
        m_tabs->setCurrentIndex(0);
    } catch (const std::exception &err) {
        QMessageBox::critical(this, tr("Failed to attach"), QString::fromUtf8(err.what()));
    }
}

void MainWindow::refreshList() {
    if (!m_entryList) {
        return;
    }
    m_entryList->clear();
    const auto &entries = m_archive.entries();
    for (const auto &entry : entries) {
        auto *item = new QListWidgetItem(QString::fromStdString(entry.name));
        const QString flags = entry.uncompressed ? tr("raw") : tr("zlib");
        item->setToolTip(tr("%1 bytes (%2)").arg(static_cast<qulonglong>(entry.data.size())).arg(flags));
        m_entryList->addItem(item);
    }
    m_removeButton->setEnabled(!entries.empty());
    m_saveButton->setEnabled(!entries.empty());
    updatePreview();
}

void MainWindow::updatePreview() {
    const int row = m_entryList->currentRow();
    const auto &entries = m_archive.entries();
    if (row < 0 || static_cast<std::size_t>(row) >= entries.size()) {
        m_preview->clear();
        m_preview->setPlaceholderText(entries.empty() ? tr("Add files or IFF payloads to start building a TRE/TRES archive")
                                                      : tr("Select an entry to view its data"));
        return;
    }

    const auto &entry = entries[static_cast<std::size_t>(row)];
    const auto bytes = format_bytes(entry.data);
    QString content = QString::fromStdString(bytes);
    content.append("\n\nPreview as text:\n");
    content.append(QString::fromUtf8(reinterpret_cast<const char *>(entry.data.data()),
                                     static_cast<int>(entry.data.size())));
    m_preview->setPlainText(content);
}

QString MainWindow::promptForArchivePath(bool saveDialog) {
    QFileDialog dialog(this, saveDialog ? tr("Save TRE/TRES") : tr("Open TRE/TRES"));
    dialog.setNameFilters(treFilters());
    dialog.setAcceptMode(saveDialog ? QFileDialog::AcceptSave : QFileDialog::AcceptOpen);
    if (!dialog.exec()) {
        return QString();
    }
    const auto selected = dialog.selectedFiles();
    if (selected.isEmpty()) {
        return QString();
    }
    return selected.first();
}

QString MainWindow::promptForPassphrase(const QString &title, const QString &label) {
    bool ok = false;
    const QString passphrase = QInputDialog::getText(this, title, label, QLineEdit::Password, {}, &ok);
    if (!ok) {
        return QString();
    }
    if (passphrase.isEmpty()) {
        QMessageBox::warning(this, tr("Passphrase required"), tr("Please provide a non-empty passphrase."));
        return QString();
    }
    return passphrase;
}

#include "MainWindow.moc"
