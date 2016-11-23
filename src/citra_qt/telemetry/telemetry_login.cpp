// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QDesktopServices>
#include <QUrl>
#include "citra_qt/telemetry/telemetry_login.h"
#include "ui_telemetry_login.h"

SubmitTestCaseLoginPage::SubmitTestCaseLoginPage(QWidget* parent) : QWizardPage(parent), ui(new Ui::SubmitTestCaseLoginPage) {
    ui->setupUi(this);
    registerField("username*", ui->username);
    registerField("password*", ui->password);

    setCommitPage(true);
}

SubmitTestCaseLoginPage::~SubmitTestCaseLoginPage() {}

void SubmitTestCaseLoginPage::initializePage() {
    wizard()->setOption(QWizard::HaveCustomButton1, true);
    setButtonText(QWizard::CustomButton1, tr("&Sign Up"));
    setButtonText(QWizard::CommitButton, tr("&Login"));
}

void SubmitTestCaseLoginPage::customButtonClicked(int button) {
    if (button == QWizard::CustomButton1)
        openSignUpUrl();
}

bool SubmitTestCaseLoginPage::validatePage() {
    // TODO(Subv): Hand off the login attempt to the Telemetry manager and report back to the user.
    setDisabled(true);
    return field("username") == "citra" && field("password") == "citra";
}

void SubmitTestCaseLoginPage::openSignUpUrl() {
    const char* signup_url = "https://discuss.citra-emu.org";
    QDesktopServices::openUrl(QUrl(signup_url));
}
