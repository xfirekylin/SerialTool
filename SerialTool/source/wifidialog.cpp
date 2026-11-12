#include "wifidialog.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFormLayout>
#include <cstring>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QTableWidget>
#include <QTextCodec>
#include <QTimer>
#include <QVBoxLayout>

WifiDialog::WifiDialog(const QString &cmdHead, const QByteArray &codecName, QWidget *parent)
    : QDialog(parent)
    , m_cmdHead(cmdHead)
    , m_codecName(codecName)
    , m_combo(0)
    , m_list(0)
    , m_connBtn(0)
    , m_discBtn(0)
    , m_collect(false)
    , m_ignoreCombo(false)
    , m_alive(true)
    , m_started(false)
    , m_switchOn(-1)
    , m_poll(0)
    , m_pollMax(0)
    , m_scanLeft(0)
    , m_sortCol(-1)
    , m_sortAsc(true)
    , m_pollTimer(new QTimer(this))
    , m_scanTimer(new QTimer(this))
{
    Qt::WindowFlags flags = Qt::Dialog;
    flags |= Qt::WindowCloseButtonHint;
    setWindowFlags(flags);
    setWindowTitle(tr("WiFi"));
    resize(480, 360);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QHBoxLayout *switchLayout = new QHBoxLayout;
    switchLayout->addWidget(new QLabel(tr("WiFi"), this));
    m_combo = new QComboBox(this);
    m_combo->addItem(tr("On"));
    m_combo->addItem(tr("Off"));
    m_combo->setCurrentIndex(-1);
    m_combo->setMinimumWidth(100);
    switchLayout->addWidget(m_combo);
    switchLayout->addStretch();
    mainLayout->addLayout(switchLayout);

    QHBoxLayout *presetLayout = new QHBoxLayout;
    presetLayout->addWidget(new QLabel(tr("Preset WiFi"), this));
    QPushButton *addBtn = new QPushButton(tr("Add"), this);
    QPushButton *refreshBtn = new QPushButton(tr("Refresh"), this);
    presetLayout->addWidget(addBtn);
    presetLayout->addWidget(refreshBtn);
    presetLayout->addStretch();
    mainLayout->addLayout(presetLayout);

    m_list = new QTableWidget(0, 3, this);
    m_list->setHorizontalHeaderLabels(QStringList()
                                      << tr("WiFi Name")
                                      << tr("Signal")
                                      << tr("Status"));
    m_list->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->verticalHeader()->setVisible(false);
    m_list->setShowGrid(false);
    m_list->horizontalHeader()->setStretchLastSection(true);
    m_list->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_list->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_list->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    mainLayout->addWidget(m_list, 1);

    QHBoxLayout *actionLayout = new QHBoxLayout;
    actionLayout->addStretch();
    m_connBtn = new QPushButton(tr("Connect"), this);
    m_discBtn = new QPushButton(tr("Disconnect"), this);
    m_connBtn->setEnabled(false);
    m_discBtn->setEnabled(false);
    m_connBtn->setMinimumWidth(100);
    m_discBtn->setMinimumWidth(100);
    actionLayout->addWidget(m_connBtn);
    actionLayout->addWidget(m_discBtn);
    actionLayout->addStretch();
    mainLayout->addLayout(actionLayout);

    m_scanTimer->setSingleShot(true);
    connect(m_combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &WifiDialog::onSwitchChanged);
    connect(addBtn, &QPushButton::clicked, this, &WifiDialog::onAdd);
    connect(refreshBtn, &QPushButton::clicked, this, &WifiDialog::onRefresh);
    connect(m_connBtn, &QPushButton::clicked, this, &WifiDialog::onConnectAp);
    connect(m_discBtn, &QPushButton::clicked, this, &WifiDialog::onDisconnectAp);
    connect(m_list, &QTableWidget::itemSelectionChanged, this, &WifiDialog::onSelectionChanged);
    connect(m_list->horizontalHeader(), &QHeaderView::sectionClicked, this, &WifiDialog::onHeaderClicked);
    connect(m_pollTimer, &QTimer::timeout, this, &WifiDialog::onPollTimeout);
    connect(m_scanTimer, &QTimer::timeout, this, &WifiDialog::onScanTimeout);
    connect(this, &QDialog::finished, this, [this](int) {
        m_alive = false;
        m_pollTimer->stop();
        m_scanTimer->stop();
    });
}

void WifiDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (m_started) {
        return;
    }
    m_started = true;
    sendCmd("wifiSw\r\n");
    QTimer::singleShot(30, this, [this]() {
        if (m_alive) {
            sendCmd("wifiList\r\n");
        }
    });
}

void WifiDialog::feedData(const QByteArray &data)
{
    if (!m_alive || data.isEmpty()) {
        return;
    }

    QByteArray chunk = data;
    chunk.replace("\r\n", "\n");
    chunk.replace('\r', '\n');
    m_rxBuf.append(chunk);

    int pos;
    while ((pos = m_rxBuf.indexOf('\n')) >= 0) {
        QByteArray line = m_rxBuf.left(pos);
        m_rxBuf.remove(0, pos + 1);
        int idx = line.indexOf("AT+POC=wifi");
        if (idx >= 0) {
            handleLine(line.mid(idx));
        }
    }
    if (m_rxBuf.size() > 8192) {
        m_rxBuf = m_rxBuf.right(1024);
    }
}

void WifiDialog::sendCmd(const QByteArray &body)
{
    if (!m_alive || body.isEmpty()) {
        return;
    }
    QByteArray cmd = m_cmdHead.toLatin1();
    cmd.append(body);
    emit sendDataRequest(cmd);
}

void WifiDialog::startPoll(int maxCount)
{
    m_poll = 0;
    m_pollMax = maxCount;
    m_pollTimer->start(1000);
}

void WifiDialog::armScan()
{
    m_scanLeft = 12;
    m_scanTimer->start(500);
}

void WifiDialog::onPollTimeout()
{
    if (!m_alive) {
        return;
    }
    m_poll++;
    if (m_poll > m_pollMax) {
        m_pollTimer->stop();
        return;
    }
    sendCmd("wifiList\r\n");
}

void WifiDialog::onScanTimeout()
{
    if (!m_alive) {
        return;
    }
    if (m_scanLeft <= 0) {
        showFail();
        return;
    }
    m_scanLeft--;
    sendCmd("wifiScan\r\n");
}

void WifiDialog::onSwitchChanged(int index)
{
    if (m_ignoreCombo) {
        return;
    }
    if (index == 0) {
        sendCmd("wifiSw&1\r\n");
    } else if (index == 1) {
        sendCmd("wifiSw&0\r\n");
    }
}

void WifiDialog::onAdd()
{
    QByteArray ssid;
    QByteArray psk;
    if (!editWifi(tr("Add Preset WiFi"), tr("Save"), QByteArray(), false, &ssid, &psk)) {
        return;
    }
    sendCmd("wifiAdd&" + psk + "&" + ssid + "\r\n");
    sendCmd("wifiList\r\n");
}

void WifiDialog::onRefresh()
{
    if (m_switchOn == 1) {
        armScan();
    } else {
        sendCmd("wifiList\r\n");
    }
}

void WifiDialog::onConnectAp()
{
    int sel = m_list->currentRow();
    if (sel < 0 || sel >= m_rows.size()) {
        return;
    }
    QByteArray ssid;
    QByteArray psk;
    if (!editWifi(tr("Connect"), tr("OK"), m_rows[sel].ssid, true, &ssid, &psk)) {
        return;
    }
    sendCmd("wifiConn&" + psk + "&" + m_rows[sel].ssid + "\r\n");
    sendCmd("wifiList\r\n");
    startPoll(4);
}

void WifiDialog::onDisconnectAp()
{
    int sel = m_list->currentRow();
    if (sel < 0 || sel >= m_rows.size()) {
        return;
    }
    sendCmd("wifiDisc&" + m_rows[sel].ssid + "\r\n");
    sendCmd("wifiList\r\n");
    startPoll(4);
}

void WifiDialog::onSelectionChanged()
{
    updateButtons();
}

void WifiDialog::onHeaderClicked(int column)
{
    if (column == m_sortCol) {
        m_sortAsc = !m_sortAsc;
    } else {
        m_sortCol = column;
        m_sortAsc = true;
    }
    QByteArray keep;
    int sel = m_list->currentRow();
    if (sel >= 0 && sel < m_rows.size()) {
        keep = m_rows[sel].ssid;
    }
    fillList(keep);
}

int WifiDialog::compareRow(const ApRow &a, const ApRow &b) const
{
    int cmp = 0;
    if (m_sortCol == 0) {
        cmp = ssidText(a.ssid).compare(ssidText(b.ssid), Qt::CaseInsensitive);
    } else if (m_sortCol == 1) {
        if (a.rssi < b.rssi) {
            cmp = -1;
        } else if (a.rssi > b.rssi) {
            cmp = 1;
        }
    } else if (a.connected < b.connected) {
        cmp = -1;
    } else if (a.connected > b.connected) {
        cmp = 1;
    }
    if (cmp == 0 && m_sortCol != 0) {
        cmp = ssidText(a.ssid).compare(ssidText(b.ssid), Qt::CaseInsensitive);
    }
    if (!m_sortAsc) {
        cmp = -cmp;
    }
    return cmp;
}

void WifiDialog::sortRows()
{
    if (m_sortCol < 0 || m_rows.size() < 2) {
        return;
    }
    for (int i = 1; i < m_rows.size(); ++i) {
        ApRow key = m_rows[i];
        int j = i - 1;
        for (; j >= 0 && compareRow(m_rows[j], key) > 0; --j) {
            m_rows[j + 1] = m_rows[j];
        }
        m_rows[j + 1] = key;
    }
}

void WifiDialog::showFail()
{
    QMessageBox::information(this, tr("WiFi"), tr("Operation failed"));
}

QString WifiDialog::ssidText(const QByteArray &ssid) const
{
    QTextCodec *utf8 = QTextCodec::codecForName("UTF-8");
    if (utf8) {
        QTextCodec::ConverterState state;
        QString text = utf8->toUnicode(ssid.constData(), ssid.size(), &state);
        if (state.invalidChars == 0) {
            return text;
        }
    }
    QTextCodec *codec = QTextCodec::codecForName(m_codecName);
    if (!codec) {
        codec = QTextCodec::codecForLocale();
    }
    if (codec) {
        return codec->toUnicode(ssid);
    }
    return QString::fromLatin1(ssid);
}

QByteArray WifiDialog::encodeText(const QString &text) const
{
    QTextCodec *codec = QTextCodec::codecForName(m_codecName);
    if (!codec) {
        codec = QTextCodec::codecForLocale();
    }
    if (codec) {
        return codec->fromUnicode(text);
    }
    return text.toUtf8();
}

bool WifiDialog::editWifi(const QString &title, const QString &okText, const QByteArray &ssidInit,
                          bool ssidReadOnly, QByteArray *ssidOut, QByteArray *pskOut)
{
    QDialog dlg(this);
    dlg.setWindowTitle(title);
    dlg.setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint | Qt::WindowTitleHint);

    QFormLayout *form = new QFormLayout(&dlg);
    QLineEdit *ssidEdit = new QLineEdit(ssidText(ssidInit), &dlg);
    ssidEdit->setReadOnly(ssidReadOnly);
    ssidEdit->setMinimumWidth(220);
    QLineEdit *pskEdit = new QLineEdit(&dlg);
    form->addRow(tr("Name"), ssidEdit);
    form->addRow(tr("Password"), pskEdit);

    QPushButton *okBtn = new QPushButton(okText, &dlg);
    okBtn->setDefault(true);
    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(okBtn);
    buttons->addStretch();
    form->addRow(buttons);

    QByteArray ssidEnc;
    QByteArray pskEnc;
    connect(okBtn, &QPushButton::clicked, &dlg, [this, &dlg, ssidEdit, pskEdit, ssidReadOnly, ssidInit, &ssidEnc, &pskEnc]() {
        QByteArray nameBytes = ssidReadOnly ? ssidInit : encodeText(ssidEdit->text());
        if (nameBytes.isEmpty()) {
            QMessageBox::information(&dlg, tr("WiFi"), tr("Name is empty"));
            return;
        }
        if (nameBytes.size() > 32) {
            QMessageBox::information(&dlg, tr("WiFi"), tr("Name is too long"));
            return;
        }
        QByteArray passBytes = encodeText(pskEdit->text());
        if (passBytes.size() < 8 || passBytes.size() > 63 || passBytes.contains('&')) {
            QMessageBox::information(&dlg, tr("WiFi"), tr("Password length must be 8-63"));
            return;
        }
        ssidEnc = nameBytes;
        pskEnc = passBytes;
        dlg.accept();
    });

    if (ssidReadOnly) {
        pskEdit->setFocus();
    } else {
        ssidEdit->setFocus();
    }
    if (dlg.exec() != QDialog::Accepted) {
        return false;
    }
    if (ssidOut) {
        *ssidOut = ssidEnc;
    }
    if (pskOut) {
        *pskOut = pskEnc;
    }
    return true;
}

void WifiDialog::updateButtons()
{
    int sel = m_list->currentRow();
    bool has = sel >= 0 && sel < m_rows.size();
    bool connected = has && m_rows[sel].connected != 0;
    m_connBtn->setEnabled(has && !connected);
    m_discBtn->setEnabled(has && connected);
}

void WifiDialog::fillList(const QByteArray &keepSsid)
{
    sortRows();
    m_list->setUpdatesEnabled(false);
    m_list->setRowCount(0);
    int keep = -1;
    for (int i = 0; i < m_rows.size(); ++i) {
        m_list->insertRow(i);
        QTableWidgetItem *nameItem = new QTableWidgetItem(ssidText(m_rows[i].ssid));
        QTableWidgetItem *rssiItem = new QTableWidgetItem(QString::number(m_rows[i].rssi));
        QTableWidgetItem *stateItem = new QTableWidgetItem(
            m_rows[i].connected ? tr("Connected") : tr("Disconnected"));
        m_list->setItem(i, 0, nameItem);
        m_list->setItem(i, 1, rssiItem);
        m_list->setItem(i, 2, stateItem);
        if (!keepSsid.isEmpty() && keepSsid == m_rows[i].ssid) {
            keep = i;
        }
    }
    if (m_sortCol >= 0) {
        m_list->horizontalHeader()->setSortIndicatorShown(true);
        m_list->horizontalHeader()->setSortIndicator(
            m_sortCol, m_sortAsc ? Qt::AscendingOrder : Qt::DescendingOrder);
    }
    m_list->setUpdatesEnabled(true);
    if (keep >= 0) {
        m_list->selectRow(keep);
    }
    updateButtons();
}

void WifiDialog::applyList()
{
    QByteArray keep;
    int sel = m_list->currentRow();
    if (sel >= 0 && sel < m_rows.size()) {
        keep = m_rows[sel].ssid;
    }
    m_rows = m_build;
    m_build.clear();
    m_collect = false;
    fillList(keep);
}

void WifiDialog::handleLine(const QByteArray &line)
{
    if (line.startsWith("AT+POC=wifiSw&")) {
        QByteArray rest = line.mid(int(strlen("AT+POC=wifiSw&")));
        if (rest.startsWith("fail")) {
            m_ignoreCombo = true;
            if (m_switchOn >= 0) {
                m_combo->setCurrentIndex(m_switchOn ? 0 : 1);
            }
            m_ignoreCombo = false;
            showFail();
            return;
        }
        if (!rest.isEmpty() && (rest.at(0) == '1' || rest.at(0) == '0')) {
            int on = rest.at(0) == '1' ? 1 : 0;
            int was = m_switchOn;
            m_ignoreCombo = true;
            m_combo->setCurrentIndex(on ? 0 : 1);
            m_ignoreCombo = false;
            m_switchOn = on;
            if (on && was != 1) {
                armScan();
            } else if (!on) {
                m_scanLeft = 0;
                m_scanTimer->stop();
            }
        }
        return;
    }
    if (line.startsWith("AT+POC=wifiScan&")) {
        QByteArray rest = line.mid(int(strlen("AT+POC=wifiScan&")));
        if (rest.startsWith("ok")) {
            m_scanLeft = 0;
            m_scanTimer->stop();
            startPoll(8);
        } else if (m_scanLeft > 0) {
            m_scanTimer->start(1000);
        } else {
            showFail();
        }
        return;
    }
    if (line.startsWith("AT+POC=wifiList&")) {
        m_build.clear();
        m_collect = true;
        return;
    }
    if (line.startsWith("AT+POC=wifiAp&")) {
        QByteArray rest = line.mid(int(strlen("AT+POC=wifiAp&")));
        if (rest == "end") {
            if (m_collect) {
                applyList();
            }
            return;
        }
        int amp1 = rest.indexOf('&');
        if (amp1 < 0) {
            return;
        }
        int amp2 = rest.indexOf('&', amp1 + 1);
        if (amp2 < 0) {
            return;
        }
        ApRow row;
        row.rssi = rest.left(amp1).toInt();
        row.connected = (rest.at(amp1 + 1) == '1') ? 1 : 0;
        row.ssid = rest.mid(amp2 + 1);
        if (m_collect) {
            m_build.append(row);
        }
        return;
    }
    if (line.startsWith("AT+POC=wifiAdd&")
            || line.startsWith("AT+POC=wifiConn&")
            || line.startsWith("AT+POC=wifiDisc&")) {
        int amp = line.indexOf('&');
        if (amp >= 0 && line.mid(amp + 1).startsWith("fail")) {
            showFail();
        }
    }
}
