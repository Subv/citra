// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWizard>
#include <QAbstractButton>

namespace Ui {
class SubmitTestCaseWizard;
}

class SubmitTestCaseWizard : public QWizard {
    Q_OBJECT

public:
    enum WizardPages {
        LoginPage = 1,
    };

    explicit SubmitTestCaseWizard(QWidget* parent);
    ~SubmitTestCaseWizard();

public slots:
    void buttonClicked(QAbstractButton* button);

private:
    std::unique_ptr<Ui::SubmitTestCaseWizard> ui;
};
