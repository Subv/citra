// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWizardPage>
#include <QAbstractButton>

namespace Ui {
class SubmitTestCaseLoginPage;
}

class SubmitTestCaseLoginPage : public QWizardPage {
    Q_OBJECT

public:
    explicit SubmitTestCaseLoginPage(QWidget* parent = nullptr);
    ~SubmitTestCaseLoginPage();

public slots:
    void customButtonClicked(int button);

protected:
    void initializePage() override;
    bool validatePage() override;

private:
    void openSignUpUrl();
    std::unique_ptr<Ui::SubmitTestCaseLoginPage> ui;
};
