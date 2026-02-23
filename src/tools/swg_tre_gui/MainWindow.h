#pragma once

#include "TreArchive.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTabWidget>
#include <QtCore/QString>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void newArchive();
    void openArchive();
    void saveArchive();
    void addFiles();
    void removeSelection();
    void selectionChanged();

    void refreshIffPreview();
    void loadDefinition();
    void saveDefinition();
    void exportIff();
    void attachIffToArchive();

private:
    QWidget *buildArchivePane();
    QWidget *buildCreationPane();
    QWidget *buildEncryptionPane();
    void refreshList();
    void updatePreview();
    QString promptForArchivePath(bool saveDialog);
    QString promptForPassphrase(const QString &title, const QString &label);

    TreArchive m_archive;
    QListWidget *m_entryList{};
    QPlainTextEdit *m_preview{};
    QPushButton *m_openButton{};
    QPushButton *m_saveButton{};
    QPushButton *m_addButton{};
    QPushButton *m_removeButton{};

    QTabWidget *m_tabs{};
    QPlainTextEdit *m_definitionEdit{};
    QPlainTextEdit *m_treePreview{};
    QPlainTextEdit *m_iffPreview{};
    QLineEdit *m_entryName{};
    QLabel *m_definitionStatus{};
    QPushButton *m_exportButton{};
    QPushButton *m_attachButton{};
    QCheckBox *m_storeUncompressed{};
    QLineEdit *m_encryptInputPath{};
    QLineEdit *m_encryptOutputPath{};
    QLineEdit *m_encryptPassphrase{};
    QLabel *m_encryptStatus{};
    QPushButton *m_encryptButton{};

    std::vector<std::uint8_t> m_lastIff;
};
