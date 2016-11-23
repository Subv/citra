// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QDesktopServices>
#include <QUrl>
#include "citra_qt/telemetry/submit_test_case_wizard.h"
#include "ui_submit_test_case_wizard.h"

SubmitTestCaseWizard::SubmitTestCaseWizard(QWidget* parent) : QWizard(parent), ui(new Ui::SubmitTestCaseWizard) {
    ui->setupUi(this);
    addPage(new QWizardPage);
    //removePage(SubmitTestCaseWizard::LoginPage);
}

SubmitTestCaseWizard::~SubmitTestCaseWizard() {}

void SubmitTestCaseWizard::buttonClicked(QAbstractButton* button) {

}
