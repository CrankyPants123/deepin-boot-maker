/*
 * Copyright (C) 2017 ~ 2018 Wuhan Deepin Technology Co., Ltd.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "bmwindow.h"
#include "view/isoselectview.h"
#include "view/usbselectview.h"
#include "view/progressview.h"
#include "view/resultview.h"
#include "bminterface.h"
#include "backend/bmhandler.h"
#include <ddialog.h>
#include <DTitlebar>
#include <DPageIndicator>
#include <DApplication>
#include <DApplicationHelper>
#include <DWindowManagerHelper>

#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <QStackedLayout>
#include <QMessageBox>
#include <QApplication>
#include <QProcess>
#include <QThread>
#include <QKeyEvent>



DWIDGET_USE_NAMESPACE

const static QString debugQSS = "background-color: rgba(255, 0, 255, 20%);border: 1px solid;"
                                " border-radius: 3; border-color: rgba(0, 255, 0, 20%);";



static void slideWidget(DWidget *left, DWidget *right)
{
    right->show();
    left->show();
    int delay = 300;
    QRect leftStart = QRect(0, 0, left->width(), left->height());
    QRect leftEnd = leftStart;
    leftEnd.setX(-left->width());

    QPropertyAnimation *animation = new QPropertyAnimation(left, "geometry");
    animation->setDuration(delay);
    animation->setStartValue(leftStart);
    animation->setEndValue(leftEnd);
    animation->start();

    QRect rightStart = QRect(left->width(), 0, right->width(), right->height());
    QRect rightEnd = leftStart;
    leftEnd.setX(0);

    QPropertyAnimation *animation2 = new QPropertyAnimation(right, "geometry");
    animation2->setDuration(delay);
    animation2->setStartValue(rightStart);
    animation2->setEndValue(rightEnd);
    animation2->start();

    animation->connect(animation, &QPropertyAnimation::finished,
                       animation, &QPropertyAnimation::deleteLater);
    animation2->connect(animation2, &QPropertyAnimation::finished,
                        animation2, &QPropertyAnimation::deleteLater);
    animation2->connect(animation2, &QPropertyAnimation::finished,
                        left, &QWidget::hide);

}

class BMWindowPrivate
{
public:
    BMWindowPrivate(BMWindow *parent): q_ptr(parent) {}

    DLabel          *m_title        = nullptr;
    DWidget         *actionWidgets  = nullptr;
    ISOSelectView   *isoWidget      = nullptr;
    UsbSelectView   *usbWidget      = nullptr;
    ProgressView    *progressWidget = nullptr;
    ResultView      *resultWidget   = nullptr;
//    DDialog         *warnDlg        = nullptr;
    BMInterface     *interface      = nullptr;
    DPageIndicator  *wsib      = nullptr;

    BMWindow *q_ptr;
    Q_DECLARE_PUBLIC(BMWindow)
};

BMWindow::BMWindow(QWidget *parent)
    : BMWindowBaseClass(parent), d_ptr(new BMWindowPrivate(this))
{
    Q_D(BMWindow);

    resize(440, 550);

    d->interface = BMInterface::instance();

    // init about info
    QString descriptionText = tr("Boot Maker is a simple tool to write system image files into CD/DVD, USB flash drive and other media.");
    QString acknowledgementLink = "https://www.deepin.org/acknowledgments/deepin-boot-maker#thanks";
    qApp->setProductName(tr("Boot Maker"));
    qApp->setApplicationAcknowledgementPage(acknowledgementLink);
//    qApp->setProductIcon(QIcon(":/theme/light/image/deepin-boot-maker.svg"));
    qApp->setProductIcon(QIcon::fromTheme("deepin-boot-maker"));
    qApp->setApplicationDescription(descriptionText);

    auto title = titlebar();
    auto flags = windowFlags() & ~Qt::WindowMaximizeButtonHint;
    setWindowFlags(flags);
#ifdef Q_OS_WIN
    title->setMenuVisible(false);
#endif
    // TODO: read it from parent
#ifdef Q_OS_MAC
    titlebar()->setWindowFlags(flags);

    QPalette pal(palette());
    pal.setColor(QPalette::Background, Qt::white);
    setAutoFillBackground(true);
    setPalette(pal);
#endif

    setTitlebarShadowEnabled(false);
    title->setTitle("");
    title->setIcon(QIcon::fromTheme("deepin-boot-maker")/*QIcon(":/theme/light/image/deepin-boot-maker.svg")*/);
//    title->setBackgroundTransparent(true);
    title->setFixedHeight(50);
//    title->setFrameStyle(QFrame::NoFrame);
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->setMargin(0);
    mainLayout->setSpacing(0);

    auto centralWidget = new DWidget;
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);

    auto viewWidth = 440;
    auto *actionsLayout = new QStackedLayout;
    mainLayout->addLayout(actionsLayout);
    actionsLayout->setMargin(0);
    d->isoWidget = new ISOSelectView();

    d->usbWidget = new UsbSelectView;
    d->usbWidget->move(viewWidth, 0);

    d->progressWidget = new ProgressView;
    d->progressWidget->move(viewWidth, 0);

    d->resultWidget = new ResultView;
    d->resultWidget->move(viewWidth, 0);

    actionsLayout->addWidget(d->isoWidget);

    actionsLayout->addWidget(d->usbWidget);
    actionsLayout->addWidget(d->progressWidget);
    actionsLayout->addWidget(d->resultWidget);

    mainLayout->addSpacing(0);
    d->wsib = new DPageIndicator(this);
    d->wsib->setAutoFillBackground(true);
    d->wsib->setPageCount(4);
    d->wsib->setCurrentPage(0);
    d->wsib->setFixedHeight(34);
    d->wsib->setPointRadius(3);
    d->wsib->setSecondaryPointRadius(3);
    d->wsib->setPointDistance(12);
    mainLayout->addWidget(d->wsib);

    slot_ThemeChange();
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::newProcessInstance, this, [ = ] {
        this->activateWindow();
    });
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged,
            this, &BMWindow :: slot_ThemeChange);
    connect(d->isoWidget, &ISOSelectView::isoFileSelected, this, [ = ] {
        slideWidget(d->isoWidget, d->usbWidget);
        setProperty("bmISOFilePath", d->isoWidget->isoFilePath());
        d->wsib->setCurrentPage(1);
        qDebug() << "iso path:" << d->isoWidget->isoFilePath();
    });

    connect(d->usbWidget, &UsbSelectView::deviceSelected, this, [ = ](const QString & partition, bool format) {
#ifdef Q_OS_WIN
        setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowMinimizeButtonHint);
        closeflags = false;
        this->setVisible(true);
#endif
        DWindowManagerHelper::instance()->setMotifFunctions(windowHandle(), DWindowManagerHelper::FUNC_CLOSE, false);
        titlebar()->setQuitMenuDisabled(true);
        slideWidget(d->usbWidget, d->progressWidget);
        d->wsib->setCurrentPage(2);
        auto isoFilePath = property("bmISOFilePath").toString();
        qDebug() << "call interface install" << isoFilePath << partition << format;
        emit d->interface->startInstall(isoFilePath, "", partition, format);
    });
    connect(d->progressWidget, &ProgressView::testCancel, this, [ = ] {
        setWindowFlags(windowFlags() | Qt::WindowCloseButtonHint);
        d->resultWidget->updateResult(BMHandler::SyscExecFailed, "title", "description");
        slideWidget(d->progressWidget, d->resultWidget);
        d->wsib->setCurrentPage(2);
    });
    connect(d->usbWidget, &UsbSelectView::finish, this, [ = ](quint32 error, const QString & title, const QString & description) {
        Qt::WindowFlags flags = Qt::WindowCloseButtonHint;
        flags |= Qt::WindowSystemMenuHint;
        flags |= Qt::WindowMinimizeButtonHint;
        titlebar()->setDisableFlags(flags);
        slideWidget(d->usbWidget, d->progressWidget);
        d->wsib->setCurrentPage(2);
        emit d->progressWidget->finish(0, error, title, description);
    });
    //diff mac，win，linux
    connect(d->progressWidget, &ProgressView::finish,
    this, [ = ](quint32 current, quint32 error, const QString & title, const QString & description) {
        qDebug() << error << title << description << current;
        if (error != BMHandler::NoError) {
            titlebar()->setMenuVisible(false);
            titlebar()->setQuitMenuDisabled(false);
#ifdef Q_OS_WIN
            setWindowFlags(Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
            closeflags = true;
            this->setVisible(true);
#endif
#ifndef Q_OS_WIN
            DWindowManagerHelper::instance()->setMotifFunctions(windowHandle(), DWindowManagerHelper::FUNC_CLOSE, true);
#endif
            d->resultWidget->updateResult(error, title, description);
            slideWidget(d->progressWidget, d->resultWidget);
            d->wsib->setCurrentPage(3);
        }
#ifdef  Q_OS_LINUX
        if (error == BMHandler::NoError && current == 101) {
            titlebar()->setMenuVisible(true);
            titlebar()->setQuitMenuDisabled(false);
            DWindowManagerHelper::instance()->setMotifFunctions(windowHandle(), DWindowManagerHelper::FUNC_CLOSE, true);
            d->resultWidget->updateResult(error, title, description);
            slideWidget(d->progressWidget, d->resultWidget);
            d->wsib->setCurrentPage(3);
        }
#endif
#ifndef Q_OS_LINUX
#if defined Q_OS_Win
        titlebar()->setQuitMenuDisabled(false);
        setWindowFlags(Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
        closeflags = true;
        this->setVisible(true);
        titlebar()->setMenuVisible(false);
#endif
        d->resultWidget->updateResult(error, title, description);
        slideWidget(d->progressWidget, d->resultWidget);
        d->wsib->setCurrentPage(3);

#endif
    });

    d->interface->start();
}

void BMWindow :: slot_ThemeChange()
{
    Q_D(BMWindow);
    DPalette pa;
    DGuiApplicationHelper::ColorType themeType = DGuiApplicationHelper::instance()->themeType();
    if (themeType == DGuiApplicationHelper::LightType) {
        pa = d->wsib->palette();
        pa.setColor(DPalette::Background, QColor(255, 255, 255));
        d->wsib->setPalette(pa);
        d->wsib->setPointColor(QColor("#2CA7F8"));
//        d->wsib->setSecondaryPointColor(QColor("#96ACBD"));
        d->wsib->setSecondaryPointColor(QColor(150, 172, 189, 51));
    } else if (themeType == DGuiApplicationHelper::DarkType) {
        pa = d->wsib->palette();
        pa.setColor(DPalette::Background, QColor("#292929"));
        d->wsib->setPalette(pa);
        d->wsib->setPointColor(QColor("#0082FA"));
//        d->wsib->setSecondaryPointColor(QColor("#555555"));
        d->wsib->setSecondaryPointColor(QColor(85, 85, 85, 102));
    }
}

BMWindow::~BMWindow()
{

}

QString rootCommand()
{
    return QString("gksu \"%1  -d -n\"").arg(qApp->applicationFilePath());
}

void startBackend()
{
    QProcess::startDetached(rootCommand());
}

#if defined(Q_OS_WIN)
void BMWindow::closeEvent(QCloseEvent *event)
{
    if (closeflags == false) {
        event->ignore();
    } else {
        event->accept();
    }
}
#endif
