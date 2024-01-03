#include "terminalview.h"
#include "ui_terminalview.h"
#include <QSettings>
#include <QTextCodec>
#include <QTimer>
#include <QKeyEvent>
#include <QDebug>
#include <QMessageBox>
#include <QRegExp>
#include <QDateTime>
#include <QFile>
#include <QDataStream>

TerminalView::TerminalView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TerminalView)
{
    ui->setupUi(this);
    lcdbuf = new uchar[240*320*2];
    memset(lcdbuf, 0, 240*320*2);
    recvlcdline = 0;
    m_resendTimer = new QTimer;
    m_asciiBuf = new QByteArray;

    ui->textEditRx->setReadOnly(true);
    ui->textEditTx->setWrap(true); // Send area auto wrap.

    setTextCodec("GB-2312"); // default

    ui->POC->setFocus();
    connect(ui->wrapLineBox, SIGNAL(stateChanged(int)), this, SLOT(onWrapBoxChanged(int)));
    connect(ui->sendButton, &QPushButton::clicked, this, &TerminalView::onSendButtonClicked);
    connect(ui->resendBox, &QCheckBox::stateChanged, this, &TerminalView::updateResendTimerStatus);
    QObject::connect(m_resendTimer, &QTimer::timeout, this, &TerminalView::sendData);
    connect(ui->resendIntervalBox, SIGNAL(valueChanged(int)), this, SLOT(setResendInterval(int)));
    connect(ui->historyBox, SIGNAL(activated(const QString &)), this, SLOT(onHistoryBoxChanged(const QString &)));
    connect(ui->wrapLineBox, SIGNAL(stateChanged(int)), this, SLOT(onWrapBoxChanged(int)));

    connect(ui->dialButton, &QPushButton::clicked, this, &TerminalView::ondialButtonClicked);
    connect(ui->gpsRun, &QPushButton::clicked, this, &TerminalView::ongpsRunClicked);
    connect(ui->log2Button, &QPushButton::clicked, this, &TerminalView::onlog2ButtonClicked);
    connect(ui->log3Button, &QPushButton::clicked, this, &TerminalView::onlog3ButtonClicked);
    connect(ui->tstButton, &QPushButton::clicked, this, &TerminalView::ontstButtonClicked);
    connect(ui->urlButton, &QPushButton::clicked, this, &TerminalView::onurlButtonClicked);
    connect(ui->pwoffButton, &QPushButton::clicked, this, &TerminalView::onpwoffButtonClicked);
    connect(ui->paramButton, &QPushButton::clicked, this, &TerminalView::onparamButtonClicked);
    connect(ui->closelog, &QPushButton::clicked, this, &TerminalView::oncloselogClicked);
    connect(ui->imeiButton, &QPushButton::clicked, this, &TerminalView::onimeiButtonClicked);
    connect(ui->iccidButton, &QPushButton::clicked, this, &TerminalView::oniccidButtonClicked);
    connect(ui->writeimei, &QPushButton::clicked, this, &TerminalView::onwriteimeiClicked);
    connect(ui->hungButton, &QPushButton::clicked, this, &TerminalView::onhungButtonClicked);
    connect(ui->rebootButton, &QPushButton::clicked, this, &TerminalView::onrebootButtonClicked);
    connect(ui->gpsNormal, &QPushButton::clicked, this, &TerminalView::ongpsNormalClicked);
    connect(ui->log1Button, &QPushButton::clicked, this, &TerminalView::onlog1ButtonClicked);
    connect(ui->apnButton, &QPushButton::clicked, this, &TerminalView::onapnButtonClicked);
    connect(ui->openGps, &QPushButton::clicked, this, &TerminalView::onopenGpsClicked);
    connect(ui->gpsSignal, &QPushButton::clicked, this, &TerminalView::ongpsSignalClicked);
    connect(ui->uartButton, &QPushButton::clicked, this, &TerminalView::onuartButtonClicked);
    connect(ui->wlanButton, &QPushButton::clicked, this, &TerminalView::onwlanButtonClicked);
    connect(ui->resetBtn, &QPushButton::clicked, this, &TerminalView::onResetBntClicked);
    connect(ui->LogAT, &QPushButton::clicked, this, &TerminalView::onLogATClicked);
    connect(ui->LogDiag, &QPushButton::clicked, this, &TerminalView::onLogDiagClicked);
    connect(ui->getParamBtn, &QPushButton::clicked, this, &TerminalView::ongetParamBtnClicked);
    connect(ui->logFileBtn, &QPushButton::clicked, this, &TerminalView::onlogFileBntClicked);
    connect(ui->logOffBtn, &QPushButton::clicked, this, &TerminalView::onlogOffBntClicked);
    connect(ui->logOnBtn, &QPushButton::clicked, this, &TerminalView::onLogOnBntClicked);
    connect(ui->setBtn, &QPushButton::clicked, this, &TerminalView::onsetBntClicked);
    connect(ui->uploadLogBtn, &QPushButton::clicked, this, &TerminalView::onUploadLogClicked);
    connect(ui->getstateBtn, &QPushButton::clicked, this, &TerminalView::onGetStateClicked);
    connect(ui->flyOnBtn, &QPushButton::clicked, this, &TerminalView::onFlyOnClicked);
    connect(ui->flyOffBtn, &QPushButton::clicked, this, &TerminalView::onFlyOffClicked);
    connect(ui->deviceInfo, &QPushButton::clicked, this, &TerminalView::onDeviceInfoClicked);
    connect(ui->factory, &QPushButton::clicked, this, &TerminalView::onFactoryClicked);
}

TerminalView::~TerminalView()
{
    delete ui;
    delete m_resendTimer;
    delete m_asciiBuf;
}

void TerminalView::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() == Qt::ControlModifier) {
        if (event->key() == Qt::Key_Return) {
            onSendButtonClicked(); // Ctrl + Enter to send.
        }
    }
}

void TerminalView::retranslate()
{
    ui->retranslateUi(this);
}

// load terminal config
void TerminalView::loadConfig(QSettings *config)
{
    bool status;
    QRadioButton *rbtn;

    config->beginGroup("Terminal");

    // set tx/rx mode
    rbtn = config->value("ReceiveMode").toString() == "Hex" ? ui->portReadHex : ui->portReadAscii;
    rbtn->setChecked(true);
    rbtn = config->value("TransmitMode").toString() == "Hex" ? ui->portWriteHex : ui->portWriteAscii;
    rbtn->setChecked(true);

    // set resend
    int msc = config->value("RepeatInterval").toInt();
    msc = msc < 10 ? 1000 : msc;
    ui->resendIntervalBox->setValue(msc);
    ui->resendBox->setChecked(config->value("ResendMode").toBool());
    setResendInterval(msc);
    updateResendTimerStatus();
    ui->rCL->setChecked(config->value("rCL").toBool());

    // set warp line
    status = config->value("RxAreaWrapLine").toBool();
    ui->wrapLineBox->setChecked(status);
    ui->textEditRx->setWrap(status);

    // load history
    loadHistory(config);

    config->endGroup();
}

void TerminalView::saveConfig(QSettings *config)
{
    QString str;

    config->beginGroup("Terminal");

    // save tx/rx mode
    str = ui->portReadHex->isChecked() ? "Hex" : "Ascii";
    config->setValue("ReceiveMode", QVariant(str));
    str = ui->portWriteHex->isChecked() ? "Hex" : "Ascii";
    config->setValue("TransmitMode", QVariant(str));
    // save resend mode
    config->setValue("RepeatInterval", QVariant(ui->resendIntervalBox->value()));
    config->setValue("ResendMode", QVariant(ui->resendBox->isChecked()));
    // save warp line
    config->setValue("RxAreaWrapLine", QVariant(ui->wrapLineBox->isChecked()));
    config->setValue("rCL", QVariant(ui->rCL->isChecked()));
    // save history
    saveHistory(config);

    config->endGroup();
}

void TerminalView::loadHistory(QSettings *config)
{
    config->beginGroup("HistoryBox");
    int count = config->beginReadArray("Items");
    for (int i = 0; i < count; ++i) {
        config->setArrayIndex(i);
        ui->historyBox->addItem(config->value("data").toString());
    }
    config->endArray();
    ui->historyBox->setCurrentIndex(0);
    config->endGroup();
}

void TerminalView::saveHistory(QSettings *config)
{
    config->beginGroup("HistoryBox");
    int count = ui->historyBox->count();
    config->beginWriteArray("Items");
    for (int i = 0; i < count; ++i) {
        config->setArrayIndex(i);
        config->setValue("data", ui->historyBox->itemText(i));
    }
    config->endArray();
    config->endGroup();
}

void TerminalView::setHighlight(const QString &language)
{
    // Send and Receive area highlight.
    ui->textEditTx->setHighLight(language);
    ui->textEditRx->setHighLight(language);
}

int tvTextEnd=1;
void TerminalView::append(const QByteArray &array)
{
    QString string;

    if (ui->portReadAscii->isChecked()) {
        arrayToString(string, array);
    } else {
        arrayToHex(string, array, 16);
    }
    if (tvTextEnd){
        QDateTime ctm;
        QString tm = ctm.currentDateTime().toString("\r[hh:mm:ss.zzz]收\u2190");
        ui->textEditRx->append(tm +string);
        lcdLineStr.clear();
        lcdLineStr.append(string);
    } else {
        lcdLineStr.append(string);
        ui->textEditRx->append(string);
    }
    if (string.endsWith("\n")){
        QStringList lcdLines = lcdLineStr.split("\r\n");
        for(int i=0;i<lcdLines.size();i++){
            lcdLineStr = lcdLines.at(i);
            int idx = lcdLineStr.indexOf("*#lcdbuf#*:");
            if (-1 != idx){
                lcdLineStr = lcdLineStr.mid(idx+strlen("*#lcdbuf#*:"));
                if (lcdLineStr.endsWith("\r\n")){
                    lcdLineStr.truncate(lcdLineStr.length() - 2);
                }

                QStringList vl = lcdLineStr.split(":");
                if (2==vl.size() && 400==vl.at(1).length()){
                    int offset = vl.at(0).toInt();
                    convertHexStr2Lcdmem(vl.at(1), offset);
                }
            }
        }
        tvTextEnd = 1;
    } else {
        tvTextEnd = 0;
    }
}

void TerminalView::sendDataRequestEx(const QByteArray &array)
{
    QString string;

    if (ui->portReadAscii->isChecked()) {
        arrayToString(string, array);
    } else {
        arrayToHex(string, array, 16);
    }
    QDateTime ctm;
    QString tm = ctm.currentDateTime().toString("\r[hh:mm:ss.zzz]发\u2192");
    ui->textEditRx->append(tm +string);

    sendDataRequest(array);
}

void TerminalView::clear()
{
    ui->textEditRx->clear();
    m_asciiBuf->clear();
}

void TerminalView::setFontFamily(QString fontFamily, int size, QString style)
{
    ui->textEditRx->setFonts(fontFamily, size, Qt::black, style);
    ui->textEditTx->setFonts(fontFamily, size, Qt::black, style);
}

void TerminalView::setPaused(bool status)
{
    m_paused = status;
    setSendButtonEnabled(!m_paused && m_sendEnabled);
    updateResendTimerStatus();
}

void TerminalView::setSendButtonEnabled(bool status)
{
    QString str;

    ui->sendButton->setEnabled(status);
    str = status ? tr("Ctrl + Enter to send") :
        tr("Connect port and start transmission to enable this button");
    ui->sendButton->setToolTip(str);
}

void TerminalView::setEnabled(bool status)
{
    m_sendEnabled = status;
    setSendButtonEnabled(!m_paused && m_sendEnabled);
    // auto resend
    updateResendTimerStatus();
}

QString TerminalView::getCmdHead(){
    if (ui->kHead->isChecked()){
        return ">";
    }
    return "AT##INRICO>";
}

// send data
void TerminalView::sendData()
{
    QByteArray array;

    if (ui->portWriteAscii->isChecked() == true) {
        QTextCodec *code = QTextCodec::codecForName(m_codecName);
        QString in = ui->textEditTx->text();
        if (ui->kHead->isChecked()){
            if (in.startsWith("AT##INRICO>"))
                in = in.mid(strlen("AT##INRICO"));
        }

        array = code->fromUnicode(in);

        if (ui->rCL->isChecked()) {
            array.append("\r\n");
        }
    } else {
        array = QByteArray::fromHex(ui->textEditTx->text().toLatin1());
    }
    sendDataRequestEx(array);
}

void TerminalView::onuartButtonClicked()
{
    QByteArray array;
    array.append("AT^CLUR\r\n");

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "警告", "关闭串口,不能打log,是否继续?",
                                  QMessageBox::Yes|QMessageBox::No);
    if (reply == QMessageBox::Yes) {

    } else {
      return;
    }

    sendDataRequestEx(array);
}

void TerminalView::onimeiButtonClicked()
{
    QByteArray array;
    array.append("AT+EGMR=0,7\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onwlanButtonClicked()
{
    QByteArray array;
    array.append("AT^WLAN\r\n");
    sendDataRequestEx(array);
}

void TerminalView::ondialButtonClicked()
{
    QByteArray array;
    array.append("ATD112;\r\n");
    sendDataRequestEx(array);
}

void TerminalView::ongpsRunClicked()
{
    QByteArray array;
    array.append("AT^GPSINTV=1,");

    QByteArray array_text;
    QTextCodec *code = QTextCodec::codecForName(m_codecName);

    array_text =code->fromUnicode(ui->textEditTx->text());

    QRegExp rx("\\d+");

    if (!rx.exactMatch(array_text)) {
        QMessageBox Msgbox;
        Msgbox.setText("请输入正确时间");
        Msgbox.exec();
        return;
    }

    array.append(array_text+"\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onlog2ButtonClicked()
{
    QByteArray array;
    array.append("AT^TRAC=2\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onlog3ButtonClicked()
{
    QByteArray array;
    array.append("AT^TRAC=3\r\n");
    sendDataRequestEx(array);
}

void TerminalView::ontstButtonClicked(){
    QByteArray array;
    array.append("AT^CTST\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onurlButtonClicked(){
    QByteArray array;
    QByteArray array_text;
    QTextCodec *code = QTextCodec::codecForName(m_codecName);

    array.append("AT^SETIP=");

    array_text =code->fromUnicode(ui->textEditTx->text());

    if (0 == array_text.length()) {
        QMessageBox Msgbox;
        Msgbox.setText("请输入IP:端口");
        Msgbox.exec();
        return;
    }

    array.append(array_text+"\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onpwoffButtonClicked(){
    QByteArray array;
    array.append("AT^PWOFF\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onparamButtonClicked(){
    QByteArray array;
    array.append("AT^PARAM\r\n");
    sendDataRequestEx(array);
}

void TerminalView::oncloselogClicked(){
    QByteArray array;
    array.append("AT^TRAC=0\r\n");
    sendDataRequestEx(array);
}

void TerminalView::oniccidButtonClicked(){
    QByteArray array;
    array.append("AT+ICCID\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onwriteimeiClicked(){
    QByteArray array;
    array.append("AT+EGMR=1,7,");

    QByteArray array_text;
    QTextCodec *code = QTextCodec::codecForName(m_codecName);

    array_text =code->fromUnicode(ui->textEditTx->text());

    if (15 != array_text.length()) {
        QMessageBox Msgbox;
        Msgbox.setText("请输入正确IMEI");
        Msgbox.exec();
        return;
    }

    array.append("\"" + array_text+"\"\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onhungButtonClicked(){
    QByteArray array;
    array.append("ATH\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onrebootButtonClicked(){
    QByteArray array;
    array.append("AT^RESTART\r\n");
    sendDataRequestEx(array);
}

void TerminalView::ongpsNormalClicked(){
    QByteArray array;
    array.append("AT^GPSINTV=0,");

    QByteArray array_text;
    QTextCodec *code = QTextCodec::codecForName(m_codecName);

    array_text =code->fromUnicode(ui->textEditTx->text());

    QRegExp rx("\\d+");

    if (!rx.exactMatch(array_text)) {
        QMessageBox Msgbox;
        Msgbox.setText("请输入正确时间");
        Msgbox.exec();
        return;
    }

    array.append(array_text+"\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onlog1ButtonClicked(){
    QByteArray array;
    array.append("AT^TRAC=1\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onapnButtonClicked(){
    QByteArray array;
    array.append("AT^APN=");

    QByteArray array_text;
    QTextCodec *code = QTextCodec::codecForName(m_codecName);

    array_text =code->fromUnicode(ui->textEditTx->text());


    if (0==array_text.length()) {
        QMessageBox Msgbox;
        Msgbox.setText("请输入正确apn");
        Msgbox.exec();
        return;
    }

    array.append(array_text+"\r\n");

    sendDataRequestEx(array);
}

void TerminalView::onopenGpsClicked(){
    QByteArray array;
    array.append("AT^GPST=0\r\n");
    sendDataRequestEx(array);
}

void TerminalView::ongpsSignalClicked(){
    QByteArray array;
    array.append("AT^GPST=1\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onResetBntClicked(){
    QByteArray array;
    array.append(getCmdHead()+"reset\r\n");
    sendDataRequestEx(array);
}
void TerminalView::onLogATClicked(){
    QByteArray array;
    array.append(getCmdHead()+"LogAT\r\n");
    sendDataRequestEx(array);

}
void TerminalView::onLogDiagClicked(){
    QByteArray array;
    array.append(getCmdHead()+"LogDiag\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onUploadLogClicked(){
    QByteArray array;
    array.append(getCmdHead()+"sosConfig\r\n");
    sendDataRequestEx(array);
}

 void TerminalView::onGetStateClicked(){
    QByteArray array;
    array.append(getCmdHead()+"state\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onFlyOnClicked(){
    QByteArray array;
    array.append(getCmdHead()+"patrolInfo\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onFlyOffClicked(){
    QByteArray array;
    array.append(getCmdHead()+"gpsinfo\r\n");
    sendDataRequestEx(array);
}

void TerminalView::ongetParamBtnClicked(){
    QByteArray array;
    array.append(getCmdHead()+"102\r\n");
    sendDataRequestEx(array);
}
void TerminalView::onlogFileBntClicked(){
    QByteArray array;
    array.append(getCmdHead()+"LogFile\r\n");
    sendDataRequestEx(array);
}
void TerminalView::onlogOffBntClicked(){
    QByteArray array;
    array.append(getCmdHead()+"log0\r\n");
    sendDataRequestEx(array);
}
void TerminalView::onLogOnBntClicked(){
    QByteArray array;
    array.append(getCmdHead()+"log1\r\n");
    sendDataRequestEx(array);
}
void TerminalView::onsetBntClicked(){
    QByteArray array;

    array.append(getCmdHead()+"apn&");

    QByteArray array_text;

    QTextCodec *code = QTextCodec::codecForName(m_codecName);

    array_text =code->fromUnicode(ui->APN->text());
    if (0==array_text.length()) {
        QMessageBox Msgbox;
        Msgbox.setText("please input APN");
        Msgbox.exec();
        return;
    }
    array.append(array_text+",");

    array_text =code->fromUnicode(ui->apnUser->text());
    if (0==array_text.length()) {

    } else {
        array.append(array_text);
    }
    array.append(",");

    array_text =code->fromUnicode(ui->apnPwd->text());
    if (0==array_text.length()) {

    } else {
        array.append(array_text);
    }
    array.append(",");

//    QRegExp rx2 ("^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
//    QRegExp rx3 ("^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])$");
    array.append("\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onDeviceInfoClicked(){
    QByteArray array;
    array.append(getCmdHead()+"deviceInfo\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onFactoryClicked(){
    QByteArray array;
    array.append(getCmdHead()+"factory\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onWrapBoxChanged(int status)
{
    ui->textEditRx->setWrap(status);
}

void TerminalView::onSendButtonClicked()
{
    QString str = ui->textEditTx->text();
    if (!str.isEmpty()) {
        sendData();

        // 数据写入历史记录下拉列表
        int i = ui->historyBox->findText(str);
        if (i != -1) { // 存在的项先删除
            ui->historyBox->removeItem(i);
        }
        // 历史记录下拉列表删除多余项
        while (ui->historyBox->count() >= 20) {
            ui->historyBox->removeItem(19);
        }
        ui->historyBox->insertItem(0, str); // 数据添加到第0个元素
        ui->historyBox->setCurrentIndex(0);
    }
}

void TerminalView::updateResendTimerStatus()
{
    bool status = m_sendEnabled && !m_paused && ui->resendBox->isChecked();

    if (status) {
        m_resendTimer->start(ui->resendIntervalBox->text().toInt());
    } else {
        m_resendTimer->stop();
    }
}

//  set resend interval time
void TerminalView::setResendInterval(int msc)
{
    m_resendTimer->setInterval(msc);
}

void TerminalView::setTextCodec(const QString &name)
{
    if (name == "GB-2312") {
        m_textCodec = GB2312;
        m_codecName = "GB-2312";
    } else if (name == "GB-18030") {
        m_textCodec = GB18030;
        m_codecName = "GB-18030";
    } else if (name == "UTF-8") {
        m_textCodec = UTF8;
        m_codecName = "UTF-8";
    } else if (name == "UTF-16BE") {
        m_textCodec = UTF16;
        m_codecName = "UTF-16BE";
    } else if (name == "UTF-16LE") {
        m_textCodec = UTF16;
        m_codecName = "UTF-16LE";
    } else { // ASCII
        m_textCodec = ASCII;
        m_codecName = "ASCII";
    }
}

void TerminalView::onHistoryBoxChanged(const QString &string)
{
    ui->textEditTx->setText(string);
}

void TerminalView::arrayToHex(QString &str, const QByteArray &array, int countOfLine)
{
    static int count;
    int len = array.length();
    str.resize(len * 3 + (len + count) / countOfLine);
    for (int i = 0, j = 0; i < len; ++i) {
        quint8 outChar = array[i], t;   //每字节填充一次，直到结束
        //十六进制的转换
        t = (outChar >> 4);
        str[j++] = t + (t < 10 ? '0' : 'A' - 10);
        t = outChar & 0x0F;
        str[j++] = t + (t < 10 ? '0' : 'A' - 10);
        str[j++] = ' ';
        if (count >= countOfLine - 1) {
            count = 0;
            str[j++] = '\n';
        } else {
            ++count;
        }
    }
}

// array转UTF8
void TerminalView::arrayToUTF8(QString &str, const QByteArray &array)
{
    int lastIndex, cut = 0;
    bool isCut = false;

    m_asciiBuf->append(array);
    lastIndex = m_asciiBuf->length() - 1;
    if (m_asciiBuf->at(lastIndex) & 0x80) { // 0xxx xxxx -> OK
        // UTF8最大编码为4字节，因此向前搜寻三字节
        for (int i = lastIndex; i >= 0 && ++cut < 4; --i) {
            uint8_t byte = m_asciiBuf->at(i);
            if (((cut < 2) && (byte & 0xE0) == 0xC0) ||
                ((cut < 3) && (byte & 0xF0) == 0xE0) ||
                (byte & 0xF8) == 0xF0) {
                isCut = true;
                break;
            }
        }
    }
    lastIndex -= isCut ? cut - 1 : -1;
    QByteArray cutArray = m_asciiBuf->mid(lastIndex);
    m_asciiBuf->remove(lastIndex, cut);
    QTextCodec *code = QTextCodec::codecForName(m_codecName);
    str = code->toUnicode(*m_asciiBuf);
    m_asciiBuf->clear();
    m_asciiBuf->append(cutArray);
}

// array转双字节编码格式(GB2312等)
void TerminalView::arrayToDualByte(QString &str, const QByteArray &array)
{
    int lastIndex;
    bool isCut = false;

    m_asciiBuf->append(array);
    lastIndex = m_asciiBuf->length() - 1;
    for (int i = lastIndex; i >= 0 && m_asciiBuf->at(i) & 0x80; --i) {
        isCut = !isCut;
    }
    if (isCut) { // 字符串最末尾的非ASCII字节数不为2的整数倍
        char ch = m_asciiBuf->at(lastIndex);
        m_asciiBuf->remove(lastIndex, 1);
        QTextCodec *code = QTextCodec::codecForName(m_codecName);
        str = code->toUnicode(*m_asciiBuf);
        m_asciiBuf->clear();
        m_asciiBuf->append(ch);
    } else {
        QTextCodec *code = QTextCodec::codecForName(m_codecName);
        str = code->toUnicode(*m_asciiBuf);
        m_asciiBuf->clear();
    }
}

// array转Unicode
void TerminalView::arrayToUTF16(QString &str, const QByteArray &array)
{
    int lastIndex;
    bool isCut = false;;

    m_asciiBuf->append(array);
    lastIndex = m_asciiBuf->length() - 1;
    isCut = (lastIndex + 1) & 0x01;
    if (isCut) { // 字符串长度不为偶数
        char ch = m_asciiBuf->at(lastIndex);
        m_asciiBuf->remove(lastIndex, 1);
        QTextCodec *code = QTextCodec::codecForName(m_codecName);
        str = code->toUnicode(*m_asciiBuf);
        m_asciiBuf->clear();
        m_asciiBuf->append(ch);
    } else {
        QTextCodec *code = QTextCodec::codecForName(m_codecName);
        str = code->toUnicode(*m_asciiBuf);
        m_asciiBuf->clear();
    }
}

// array转ASCII
void TerminalView::arrayToASCII(QString &str, const QByteArray &array)
{
    if (m_asciiBuf->isEmpty()) {
        str = QString::fromLatin1(array);
    } else {
        m_asciiBuf->append(array);
        str = QString::fromLatin1(*m_asciiBuf);
        m_asciiBuf->clear();
    }
}

// 这个函数可以避免中文接收的乱码
void TerminalView::arrayToString(QString &str, const QByteArray &array)
{
    switch (m_textCodec) {
    case GB2312:
    case GB18030:
        arrayToDualByte(str, array);
        break;
    case UTF8:
        arrayToUTF8(str, array);
        break;
    case UTF16:
        arrayToUTF16(str, array);
        break;
    default: // ASCII
        arrayToASCII(str, array);
        break;
    }
}

// 保存文件
void TerminalView::saveText(const QString &fname)
{
    QFile file(fname);

    if (file.open(QFile::WriteOnly)) {
        file.write(ui->textEditRx->text().toUtf8());
        file.close();
    }
}

void TerminalView::setTabsInsertSpaces(bool enable)
{
    ui->textEditTx->setIndentationsUseTabs(!enable);
}

void TerminalView::setTabWidth(int width)
{
    ui->textEditTx->setTabWidth(width);
}

void TerminalView::setAutoIndent(bool enable)
{
    ui->textEditTx->setAutoIndent(enable);
}

void TerminalView::setIndentationGuides(bool enable)
{
    ui->textEditTx->setIndentationGuides(enable);
}

void TerminalView::on_gpsLogOff_clicked()
{
    QByteArray array;
    array.append(getCmdHead()+"Lvc1\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_download_clicked()
{
    QByteArray array;
    array.append(getCmdHead()+"autoDl\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_getApn_clicked()
{
    QByteArray array;
    array.append(getCmdHead()+"getApn\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_sendCmd_clicked()
{
    QByteArray array;

    QByteArray cmds = ui->cmdlist->currentText().toUtf8();
    if (cmds == "simKey"){
        array.append(getCmdHead()+"simKey,2,100\r\n");
    }else if (cmds == "assertMode"){
        array.append(getCmdHead()+"assertMode*I0,-1\r\n");
    }else if (cmds == "at2poc"){
        array.append(getCmdHead()+"poc&AT+l\r\n");
    }else if (cmds == "at2UI"){
        array.append(getCmdHead()+"poc>+atrsp\r\n");
    }else if (cmds == "Account"){
        array.append(getCmdHead()+"t1&1185202675569392&123&zh.inrico.cn\r\n");
    } else if (cmds == "simuKeyInput"){
        array.append(getCmdHead()+"keySimu,*#33*#,\r\n");
    } else {
        array.append(getCmdHead()+cmds+ "\r\n");
    }
    sendDataRequestEx(array);
}

void TerminalView::on_relogin_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("relogin\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_keyMenu_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,s,\r\n");
    sendDataRequestEx(array);

}

void TerminalView::on_keyUp_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,u,\r\n");
    sendDataRequestEx(array);
}


void TerminalView::on_keyCancel_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,c,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_keyLeft_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,L,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_keyOk_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,o,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_keyRight_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,r,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_keyCall_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,g,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_keyDown_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,d,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_keyRed_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,e,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_key1_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,1,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_key2_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,2,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_key3_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,3,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_key5_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,5,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_key4_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,4,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_key6_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,6,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_key7_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,7,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_key8_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,8,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_key9_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,9,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_keyStar_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,*,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_key0_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,0,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_keyHash_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,#,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_keyPtt_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,p,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_keySos_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,a,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_keyHome_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,h,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_keyF1_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,x,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_keyF2_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,y,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_keyF3_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,z,\r\n");
    sendDataRequestEx(array);
}

void TerminalView::convertHexStr2Lcdmem(const QString &src, int offset){
    int len = 200;
    QByteArray ba = QByteArray::fromHex(src.toStdString().data());
    if (len > ba.size()){
        len = ba.size();
    }

    recvlcdline++;
    memcpy(lcdbuf+offset, ba.data(), len);
    if (153400==offset){
        displayLcdScreen();
        recvlcdline = 0;
    }
}

void TerminalView::displayLcdScreen()
{
    // Load the image data into the bytes array

    QImage image((uchar *)lcdbuf, 240, 320, QImage::Format_RGB16);
    ui->lcd->setPixmap(QPixmap::fromImage(image));
    ui->lcd->show();
}

void TerminalView::on_lcdfresh_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("lcdBuf\r\n");
    sendDataRequestEx(array);
}

void TerminalView::SaveBinFile() {
    const char *lcdbufstr[] ={
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045009EF7450045009EF79EF79EF79EF79EF74500450045009EF74500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500C400430283028302",
    "4302C40045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045009EF79EF7450045009EF7450045004500450045009EF79EF79EF74500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045008400C302E204E204E204E204E204E204C302840045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004F63FFFF7ACE450056A5FFFF56A5450045004500450045004500450045004500450045009EF745009EF7450045009EF745004500450045004500450045009EF745004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500A203E204E204E204E204E204E204E204E204A2034500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045009DEFDEF7A6109DEFDEF707194C42B7AD97AD4C4245004500450045004500450045009EF745009EF7450045009EF745009EF79EF79EF74500450045009EF74500450045004500450045004500450045004500450045004500450045004500117CFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
    "FFFFFFFFFFFFFFFFFFFFFFFF45004500450045004500450045008302E204E204E204E204E204E204E204E204E204E2048302450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500A610F8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDAA29450045004500450045004500450045004500F494FFFFD7B5FFFF7ACE0719DEF7BEF7BEF7DEF728194500450045004500",
    "450045009EF79EF79EF79EF745009EF74500450045009EF74500450045009EF7450045004500450045004500450045004500450045004500450045004500117CFFFFFFFF4500FFFFFFFF4500FFFFFFFF4500FFFFFFFFFFFFFFFFFFFF45004500450045004500450045004204E204E204E2044204040204024204E204E204E204420445004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500127CAD4A45004500",
    "450045004500450045004500450045004500450045004500450045004500F8BD450045004500450045004500450045004500A608DEF7FFFFFFFF6C4AEE52FFFF97AD97ADFFFF0E5B450045004500450045004500450045009EF7450045009EF74500450045009EF74500450045009EF7450045004500450045009EF79EF79EF7450045004500450045004500117CFFFFFFFFFFFF4500FFFFFFFF4500FFFFFFFF4500F8BD4F63281928192819450045004500450045004500C400E204E204E204E204C40045004500",
    "C400E204E204E204E204C4004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500127C2B3A45004500450045004500450045002819450045004500450045004500450045004500F8BD450045004500450045004500450045004500450018BEFFFF5DE74500C710BEF7DEF7DEF7DEF7E710450045004500450045004500450045009EF7450045009EF79EF79EF79EF79EF74500450045009EF74500450045004500",
    "45009EF79EF79EF7450045004500450045004500FFFFFFFFFFFFFFFF450045004500450045004500450045002410A6B925C925C9A6B9241045004500450045006401E204E204E204420445004500450045000204E204E204E2046401450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500127C2B3A4500450045002E5BAA2945004500127C7ACE2B3A4500450045004500450045004500F8BDF8BDAA2945004500",
    "4500450045004500450045002819906BEA3145004500E710738473840719450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045009EF79EF79EF7450045004500450045004500FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF4F63241861C0EFE3FFFFFFFF18F604C924184500450045008400E204E204E204E2042401450045002401E204E204E204E204840045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500127C2B3A450045004500281996AD7DEF159DF8BDFFFFFFFF159DA61045004500450045004500F8BDFFFF127C4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045009EF79EF79EF745009EF79EF79EF7450045004500450045004500FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
    "281986B9EFE361C896EDFFFFFFFF18F686B945004500450045004204E204E204E2048204C302C3028204E204E204E2044204450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500127C2B3A450045004500450045002B3AFBDEFFFFFFFF127CF8BD7DEF906B4500450045004500F8BDFFFF127C45004500450045004500450045007DEFFFFF4500F494FFFFFFFFFFFFDBD676ADFFFFFFFFFFFF450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045009EF79EF79EF745009EF79EF79EF7450045004500450045004500FFFFFFFF45004500450045004500450045004500450025C9FFFF96ED61C896EDFFFFFFFF25C945004500450045000402E204E204E204E204E204E204E204E204E204E204040245004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500127C2B3A45004500",
    "4500450045004500A610127CFFFF45004500A610AD4A2B3A450045004500F8BDF8BDAA2945004500450045004500450045007DEFFFFF4500A92976ADFFFFDBD66C4A76ADFFFF159D0B3A4500450045004500450045009EF745009EF7450045004500450045004500450045004500450045009EF79EF79EF745009EF79EF79EF7450045004500450045004500FFFFFFFFFFFFFFFFFFFF4500FFFFFFFF4500FFFF281925C9FFFFFFFF96ED61C896EDFFFF25C9450045004500450045002303E204E204E204E204E204",
    "E204E204E2042303450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500127C2B3A45004500450045004500450045004500281945004500450045004500450045004500F8BD4500450045004500450045004500450045007DEFFFFF45004500906BFFFFF8BD450076ADFFFFFFFF3CE7450045004500450045009EF79EF745009EF745004500450045004500450045009EF79EF79EF745009EF79EF79EF7",
    "45009EF79EF79EF7450045004500450045004500FFFFFFFFFFFFFFFFFFFF4500FFFFFFFF4500FFFF281986B918F6FFFFFFFF96ED61C8EFE386B94500450045004500450045000204E204E204E204E204E204E2040204450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500127CAD4A45004500450045004500450045004500450045004500450045004500450045004500F8BD4500450045004500",
    "450045004500450045007DEFFFFF45004500906BFFFFF8BD450076ADFFFF96AD4B4245004500450045009EF79EF79EF745009EF79EF79EF7450045004500450045009EF79EF79EF745009EF79EF79EF745009EF79EF79EF7450045004500450045004500FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF4F63241825C118F6FFFFFFFFCFE361C0241845004500450045004500450084000204E204E204E204E2040204840045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500A610F8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDF8BDAA294500450045004500450045004500450045007DEFFFFFFFFF5284906BFFFFF8BD450076ADFFFFFFFFDEF74500450045004500450045009EF745009EF79EF7450045009EF79EF79EF745009EF79EF79EF745009EF79EF79EF745009EF79EF79EF74500450045004500450045004500450045004500450045004500450045004500",
    "450045002410A6B925C925C9A6B924104500450045004500450045004500450024018204E204E20482042401450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500CD52906B906B69212819906B0B3A4500CA31906B906B906B450045004500",
    "4500450045009EF745009EF74500450045009EF79EF79EF745009EF79EF79EF745009EF79EF79EF745009EF79EF79EF745009EF79EF79EF7450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002401E204E2042401450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500A401A401",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500230001000000000000000000000000000100230044004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450024002200010001000100230045004500450045004500450045004500450045004500450045004500450045004500240022000100010001002300450045004500450045004500450045004500450045004500450045004500450045004500450045004500450023000000E73954A53CE7DEF7DEF7DEF7DEF79DEF58C64C636108220045004500450045004500450045004500450000000000000000000000000000000000",
    "0000000000000000000000000000000000002200450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500240001008210CE73B6B5F7BDF39C083A230045004500450045004500450045004500450045004500450045004500240001008210CE73B6B5F7BDF39C083A2300450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045000100E739BAD6FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF9DEF4C63010045004500450045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450024002300010000006529139DBEF7FFFF",
    "FFFFFFFFFFFFEF7B2200450045004500450045004500450045004500450024002300010000006529139DBEF7FFFFFFFFFFFFFFFFEF7B220045004500450045004500450045004500450045004500450045004500450045004500450024002100718CFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF34A521004500450045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7B2200450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002400E4184D6BD6B57DEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045002400E4184D6BD6B57DEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045004500450024006208D7BDFFFFFFFFFFFFFFFFFFFFBEF758C655AD95ADDAD6DEF7FFFF",
    "FFFFFFFFFFFFFFFF718C0100450045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7B2200450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFF",
    "FFFFFFFFFFFFFFFFFFFFFFFFEF7B220045004500450045004500450045004500450045004500450045004500450045004500420818C6FFFFFFFFFFFFFFFFBEF7CF7B41080100010001000000242159CEFFFFFFFFFFFFFFFFFFFFC631240045004500450045000000FFFFFFFFFFFFFFFFFFFF9DEF7DEF7DEF7DEF7DEF7DEF7DEF7DEF7DEF7DEF7DEFDAD60C63230045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045004500450022003084FFFFFFFFFFFFFBDE45290200440045004500450045002400410879CEFFFFFFFFFFFFFFFF95AD010045004500450045000000FFFFFFFFFFFFFFFFFFFF610800000000",
    "0000000000000000000000000000000000002300450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002400C739918C95AD95AD95AD95ADBAD6FFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045002400C739918C95AD95AD95AD95ADBAD6FFFFFFFFFFFFFFFFEF7B2200450045004500450045004500450045004500450045004500",
    "4500450045004500450045000100CE733CE7BAD6A210230045004500450045004500450045002300C739FFFFFFFFFFFFFFFFDEF7821024004500450045000000FFFFFFFFFFFFFFFFBEF7000045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500240002000100010001000000EF7BFFFF",
    "FFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500240002000100010001000000EF7BFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045004500450045004500220061086208240045004500450045004500450045004500450000009AD6FFFFFFFFFFFFFFFF284223004500450045000000FFFFFFFFFFFFFFFF1BDF00004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B220045004500450045004500450045004500450045004500450045004500450045004500450045004500440044004500450045004500450045004500450045004500",
    "0100F39CFFFFFFFFFFFFFFFF4D6B23004500450045000000FFFFFFFFFFFFFFFF59CE01004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B2200450045004500450045004500450045004500450045004500450045004500",
    "2200EF7BFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045000200000000000000020045004500450045004500450045004500450045004500450045004500450045004500450022001084FFFFFFFFFFFFFFFFCF7B22004500450045000000FFFFFFFFFFFFFFFFB6B50100450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B2200450045004500450045004500450045004100918CBEF7DEF7BEF7918C41004500450045004500450045004500450045004500450045004500450045004500450022003084FFFFFFFFFFFFFFFFAE7322004500450045000000FFFFFFFFFFFFFFFF139D010045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B220045004500450045004500450045002200518CFFFFFFFFFFFF",
    "FFFFFFFF518C220045004500450045004500450045004500450045004500450045004500450045000100139DFFFFFFFFFFFFFFFFCB5A23004500450045000000FFFFFFFFFFFFFFFF518C220045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFF",
    "FFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B2200450045004500450045004500450000005CE7FFFFFFFFFFFFFFFFFFFF3CE7000045004500450045004500450045004500450045004500450045004500450045000000BAD6FFFFFFFFFFFFFFFF652924004500450045000000FFFFFFFFFFFFFFFFAE7300000000000000000000000000000100230045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B2200450045004500450045004500450000009DEFFFFFFFFFFFFFFFFFFFFF9DEF00004500450045004500450045004500450045004500450045004500450045002400",
    "2421FFFFFFFFFFFFFFFF5CE7000045004500450045000000FFFFFFFFFFFFFFFFD29417BE7DEFDEF7DEF7DEF7DEF75CE7B6B5694A20002300450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B2200450045004500450045004500450045004500450045004500450045004500",
    "2200EF7BFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500010055ADFFFFFFFFFFFFFFFFFFFF54A5010045004500450045004500450045004500450045004500450045004500450022001084FFFFFFFFFFFFFFFFD39C020045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF1BDF484201004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B220045004500450045004500450045002400C31058C6FFFFFFFFFFFF58C6C310240045004500450045004500450045004500450045004500450045004500440061085CE7FFFFFFFFFFFFFFFFA631240045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFFBEF77DEF",
    "BEF7FFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7B0100450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B2200450045004500450045004500450045002400010004210842",
    "2421010024004500450045004500450045004500450045004500450045004500450045000200EF7BFFFFFFFFFFFFFFFF17BE0100450045004500450045000100B6B5FFFFFFFFDEF775AD8529000000000000C318718CDEF7FFFFFFFFFFFFFFFFFFFF4D6B22004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFF",
    "FFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500240023002400450045004500450045004500450045004500450045004500450045004500450024000421BEF7FFFFFFFFFFFFFFFFE739230045004500450045004500450021008E73518C65290100240045004500450024000100A6317DEFFFFFFFFFFFFFFFFFBEF70421240045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B2200450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002100F7BDFFFF",
    "FFFFFFFFFFFF54A5010045004500450045004500450045004500220022002400450045004500450045004500450022004842FFFFFFFFFFFFFFFFFFFFB294020045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B2200450045004500450045004500450045004500450045004500450045004500",
    "2200EF7BFFFFFFFFFFFFFFFFEF7B220045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045000200CE73FFFFFFFFFFFFFFFF7DEFE318240045004500450045004500450045004500450045004500450045004500450045004500450045000100D7BDFFFFFFFFFFFFFFFF7DEF21004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B220045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450023000842DEF7FFFFFFFFFFFFFFFFAA5223004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045002300EB5AFFFFFFFFFFFFFFFFFFFFA6312400450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B2200450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500240024217DEFFFFFFFFFFFFFFFFFD294010045004500450045004500450045004500450045004500450045004500450045004500450045004500450024002421FFFFFFFFFFFFFFFFFFFFEB5A230045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFF",
    "FFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B220045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002400C310FBDEFFFFFFFFFFFFFFFF58C64108450045004500450045004500450045004500450045004500450045004500450045004500450045004500450044004108FFFFFFFFFFFFFFFFFFFFAE7322004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002400821079CEFFFFFFFFFFFFFFFF1BDF",
    "E3182400450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045000000FFFFFFFFFFFFFFFFFFFFCF7B22004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B2200450045004500450045004500450045004500450045004500450045004500",
    "2200EF7BFFFFFFFFFFFFFFFFEF7B2200450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002400610838C6FFFFFFFFFFFFFFFF7DEF65292300450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002400A210FFFFFFFFFFFFFFFFFFFF6D6B2200450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B220045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002400820838C6FFFFFFFFFFFFFFFFBEF7E739230045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450023002842FFFFFFFFFFFFFFFFFFFF694A2300450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B2200450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045002400A21058C6FFFFFFFFFFFFFFFFBEF72842220045004500450045004500450045004500450045004500240023002400450045004500450045004500450045004500450045000200918CFFFFFFFFFFFFFFFFFFFFC318240045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFF",
    "FFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B2200450045004500450045004500450045004500220000000000000022004500450045004500450045002300C318BAD6FFFFFFFFFFFFFFFFBEF7494A2200450045004500450045004500450045004500450045002300A631AA52252123004500450045004500450045004500450045002400A2109DEFFFFFFFFFFFFFFFFF38C6010045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B220045004500450045004500450045004500210050849DEFDEF79DEF308421004500450045004500230024211BDFFFFFFFFFFFFFFFFFBEF7494A2200450045004500",
    "450045004500450045004500450045002400A2101BDFDEF77DEFE739010044004500450045004500450045002400000095ADFFFFFFFFFFFFFFFFFFFFEB5A230045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFEF7B2200450045004500450045004500450045004500450045004500450045004500",
    "2200EF7BFFFFFFFFFFFFFFFFEF7B220045004500450045004500450045002200EF7BFFFFFFFFFFFFFFFFFFFFEF7B2200450045002200C6317DEFFFFFFFFFFFFFFFFF9DEF08420000000000000000000000000000000000000000000000000000210096B5FFFFFFFFFFFFDEF7718C6108010023002400440024000200610875ADFFFFFFFFFFFFFFFFFFFF79CE210045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045000000000000000000000000000000EF7BFFFFFFFFFFFFFFFFEF7B00000000000000000000000045004500450045000000000000000000000000000000EF7BFFFFFFFFFFFFFFFFEF7B0000000000000000000000004500450000001BDFFFFFFFFFFFFFFFFFFFFF1BDF0000450023008A52BEF7FFFFFFFFFFFFFFFFFFFFBEF79DEFDEF7DEF7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF200034A5FFFFFFFFFFFFFFFFFFFFBEF775ADCB5A04216108",
    "6529518C7DEFFFFFFFFFFFFFFFFFFFFF9DEF6529240045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF0000450045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF000045000000BEF7FFFFFFFFFFFF",
    "FFFFFFFFBEF7000045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF000001000F7CDEF7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF9DEF073A230045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF0000450045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00004500010038C6FFFFFFFFFFFFFFFFFFFF17BE010045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF000045000100C739BAD6FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFBAD64529230045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF0000450045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF000045002400C7399DEFFFFFFFFFFFFF9DEFC731240045002200EF7BF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BD",
    "F7BDF7BDF7BDF7BDF7BDF7BDF7BDEF7B22004500450023000000494A38C6DEF7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFDBDE0B5B21002400450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EF7BF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDEF7B2200450045002200EF7BF7BDF7BDF7BDF7BDF7BD",
    "F7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDF7BDEF7B2200450045002300E318EF7B75ADEF7BE3182300450045004500220001000100010001000100010001000100010001000100010001000100010001000100010001000100220045004500450045004500230001004108E7390F7C34A5B6B5B6B5F39C2C63E318000023004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450022000100010001000100010001000100010001000100010001000100010001000100010022004500450045004500220001000100010001000100010001000100010001000100010001000100010001000100220045004500450045002400220001002200240045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002300220001000100",
    "0100010023002400450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045002400230001000100220024004500450045004500450045004500450045004500450045004500450045004500450045002400230001000100220024004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002400020001002200450045004500450045004500450045002400020001002200450045004500450045004500450045004500450045004500450024002300010001002200240045004500450045004500",
    "4500240023000100010022002400450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002300000000002200450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200A2102C6354A596B5518C8631010024004500450045004500240001000000000000000000230045004500450045002200A2102C6354A596B5518C8631010024004500450045004500220000000000000000000000230045004500450045004500450045004500450045004500450023000100",
    "2421918CF7BDEF7B220045004500450045004500230001002421918CF7BDEF7B2200450045004500450045004500450045004500450045002200A2102C6354A596B5518C8631010024004500450045002200A2102C6354A596B5518C8631010024004500450045004500450045002300000000000000450045004500450024000000000000002400450045004500450001000000000024004500450045004500450045004500450045004500450045004500450045004500450023000C63FFFFFFFFEF7B22004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002100CF7BBEF7FFFFFFFFFFFFFFFFFFFFB6B562084400450045002300041975ADBEF7DEF7DEF7",
    "FBDE0C630100450045002100CF7BBEF7FFFFFFFFFFFFFFFFFFFFB6B562084400450024004100AE731BDFDEF7DEF7DEF799CE894A0100450045004500450045004500450045004500450023008A5217BEDEF7FFFFFFFFFFFF0000450045004500450023008A5217BEDEF7FFFFFFFFFFFF000045004500450045004500450045004500450045002100CF7BBEF7FFFFFFFFFFFFFFFFFFFFB6B56208440045002100CF7BBEF7FFFFFFFFFFFFFFFFFFFFB6B56208440045004500450045002300AA52FFFFFFFFBAD60000",
    "450045002400A210FFFFFFFFFFFFA210240045004500000079CEFFFFFFFFA6312400450045004500450045004500450045004500450045004500450045004500450023000C63FFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500220034A5FFFFFFFFFBDECE73AE731BDFFFFFFFFF54A501004500240024215CE7FFFFFFFF9DEFDEF7FFFFFFFF518C0100220034A5FFFFFFFFFBDECE73AE731BDFFFFFFFFF54A501002400C31879CEFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFF000045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFF0000450045004500",
    "450045004500450045004500220034A5FFFFFFFFFBDECE73AE731BDFFFFFFFFF54A50100220034A5FFFFFFFFFBDECE73AE731BDFFFFFFFFF54A50100450045004500450024000319FFFFFFFFDEF761084400450023008A52FFFFFFFFFFFFAA522300450045002100BEF7DEF7BEF741084500450045004500450045004500450045004500450045004500450045004500450023000C63FFFFFFFFEF7B2200450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200518CBAD675AD210022002200A2107DEFFFFFFFFF45292400010017BEFFFFFFFF0F7C000086319DEFFFFFFFFFE7390100518CBAD675AD210022002200A2107DEFFFFFFFFF452902003084FFFFFFFF",
    "38C6073AE318D294FFFFFFFFDEF704212400450045004500450045004500450045002300C7394C6395ADFFFFFFFFFFFF000045004500450045002300C7394C6395ADFFFFFFFFFFFF00004500450045004500450045004500450045002200518CBAD675AD210022002200A2107DEFFFFFFFFF45292200518CBAD675AD210022002200A2107DEFFFFFFFFF45292400450045004500450000003CE7FFFFFFFFC7392400450002009294FFFFFFFFFFFFB2940200450024008631FFFFFFFF58C601004500450045004500",
    "450045004500450045004500450045004500450045004500450023000C63FFFFFFFFEF7B2200450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45002200410042004400450045000100F39CFFFFFFFF2C632200C739FFFFFFFF9AD6000045000200EF7BFFFFFFFF95AD01002200410042004400450045000100F39CFFFFFFFF2C63220042084D6B2C630000230024000000F7BDFFFFFFFF4C63230045004500450045004500450045004500450023000100EF7BFFFFFFFFFFFF00004500450045004500450023000100EF7BFFFFFFFFFFFF000045004500450045004500450045004500450045002200410042004400450045000100F39CFFFFFFFF2C6323002200",
    "410042004400450045000100F39CFFFFFFFF2C6323004500450045004500010096B5FFFFFFFF4C6323004500000099CE9DEFBEF7DEF7BAD60000450023000C63FFFFFFFFB29402004500240023000100010022002400450045004500450045004500240022000100010002000C63FFFFFFFFEF7B220045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450022000F7CFFFFFFFFCF7B01003084FFFFFFFFCE732200450024002421FFFFFFFFBEF7610844004500450045004500450022000F7CFFFFFFFFCF7B220045002200230045004500450022001084FFFFFFFFCF7B220045004500450045004500450045004500450045002200",
    "EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF0000450045004500450045004500450045004500450045004500450045004500450022000F7CFFFFFFFFCF7B220045004500450045004500450022000F7CFFFFFFFFCF7B220045004500450045002200EF7BFFFFFFFFD29402002400A210FFFFFFFFDAD6FFFFFFFFE318240002009294FFFFFFFF2C632300220082102C6354A595ADEF7B24210100450045004500450002000421EF7B95AD139D694A2C63FFFFFFFFEF7B22004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045000200B294FFFFFFFF4D6B0000F7BDFFFFFFFFE739230045004500",
    "00005CE7FFFFFFFFE7392300450045004500450045000200B294FFFFFFFF4D6B23004500450045004500450045000100B294FFFFFFFFCB5A230045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF000045004500450045004500450045004500450045004500450045004500450045000200B294FFFFFFFF4D6B23004500450045004500450045000200B294FFFFFFFF4D6B230045004500450045002300494AFFFFFFFF58C6",
    "01002300AA52FFFFFFFF34A5FFFFFFFFEB5A2300010017BEFFFFFFFF863101004D6BBEF7FFFFFFFFFFFFFFFFFFFF718C21004500450021005084DEF7FFFFFFFFFFFFFFFF5CE7FFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500000099CEFFFFFFFFA63100001BDFFFFFFFFFE318240045004500010038C6FFFFFFFFEB5A230045004500450045004500000099CEFFFFFFFFA6312400450045004500450024002200C3185CE7DEF77DEF8210240045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF0000450045004500",
    "4500450045004500450045004500450045004500450045004500000099CEFFFFFFFFA6312400450045004500450045004500000099CEFFFFFFFFA631240045004500450045002400A210FFFFFFFFBEF7200002009294FFFFFFFFCB5AFFFFFFFFF39C010000009DEFDEF7BEF72000AE73FFFFFFFF59CE2842494ABAD6FFFFFFFFCF7B220002003084FFFFFFFFBEF734A5918CDBDEFFFFFFFFFFFFEF7B2200450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002400C631FFFFFFFFDBDE00000000BEF7FFFFFFFF41084400450045000100B6B5FFFFFFFFAE73220045004500450045002400C631FFFFFFFFDBDE00004500450045004500",
    "000004218E735CE7DEF7BEF7694A2300450045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF00004500450045004500450045004500450045004500450045004500450045002400C631FFFFFFFFDBDE00004500450045004500450045002400C631FFFFFFFFDBDE00004500450045004500450045000000FBDEFFFFFFFF6529000099CEDEF73CE74108BEF7DEF7FBDE00002421FFFFFFFF38C64529DEF7DEF758C621002300",
    "2300C2109DEFDEF79DEF61084529DEF7DEF7BEF7A631010002000000D294FFFFFFFFEF7B2200450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045000000D6B5FFFFFFFF4D6B22000000FFFFFFFFFFFF0000450045004500010075ADFFFFFFFFEF7B220045004500450045000000D6B5FFFFFFFF4D6B22004500450045000000FFFFFFFFFFFFFFFF7DEF073A01004500450022000100010001000100010022004500450045002200EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF00004500450045002200010001000100010001002200450045004500450045000000D6B5FFFFFFFF4D6B220045004500",
    "45004500450045000000D6B5FFFFFFFF4D6B2200450045004500450045004500010054A5FFFFFFFFEB5AA210FFFFFFFF54A50000F7BDFFFFFFFF0421AA52FFFFFFFF9294B294FFFFFFFF2842230045004500010054A5FFFFFFFF494A918CFFFFFFFF518C01004500450023000C63FFFFFFFFEF7B220045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002200EB5AFFFFFFFF99CE210045000000DEF7FFFFFFFF0000450045004500010075ADFFFFFFFFCE7322004500450045002200EB5AFFFFFFFF99CE2100450045004500450023002C6395ADBAD6DEF7DEF7DEF7718C21002200EF7BF7BDF7BDF7BDF7BDF7BDEF7B2200450045002200",
    "EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF0000450045002200EF7BF7BDF7BDF7BDF7BDF7BDEF7B22004500450045002200EB5AFFFFFFFF99CE21004500450045004500450045002200EB5AFFFFFFFF99CE2100450045004500450045004500450022008E73FFFFFFFF718C8A52FFFFFFFF6D6B01000F7CFFFFFFFF0B5B3084FFFFFFFF0B5BBAD6DEF7DEF7200000000000000000003084FFFFFFFF8E7399CEFFFFFFFF863124004500450023000C63FFFFFFFFEF7B22004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500240024219DEFDEF7DEF7A6312300450000005CE7FFFFFFFF6108440045004500",
    "0100D6B5FFFFFFFF4D6B230045004500240024219DEFDEF7DEF7A6312300450045004500450045002300010000000421DAD6FFFFFFFF139D0000FFFFFFFFFFFFFFFFFFFFFFFFFFFF0000450045002200EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF0000450045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFF000045004500240024219DEFDEF7DEF7A631230045004500450045004500240024219DEFDEF7DEF7A631230045004500450045004500450045002300E739FFFFFFFF",
    "D7BD718CFFFFFFFF652923000842FFFFFFFFD294B6B5FFFFFFFF6529BEF7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7BBEF7FFFFFFFF410844004500450023000C63FFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002400A210DAD6FFFFFFFF6D6B220045004500000079CEFFFFFFFF2421240045004500000099CEFFFFFFFF694A230045002400A210DAD6FFFFFFFF6D6B22004500450045004500450045004500450045002300073AFFFFFFFFFFFF863100000000000000000000000000004500450045002200EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF0000450045004500",
    "0000000000000000000000000000450045002400A210DAD6FFFFFFFF6D6B2200450045004500450045002400A210DAD6FFFFFFFF6D6B22004500450045004500450045004500450044006108DEF7DEF75CE738C6DEF77DEF000044004108BEF7DEF799CE1BDFDEF79DEF0000BEF7FFFFFFFF79CE79CE79CE79CE79CE79CE79CE75AD073AFFFFFFFFFFFF000045004500450023000C63FFFFFFFFEF7B2200450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002400610838C6FFFFFFFFD29401004500450045000100139DFFFFFFFF694A2300450045002000BEF7FFFFFFFF031924002400610838C6FFFFFFFFD29401004500450045004500450045004500",
    "45004500450045002000FFFFFFFFFFFF8D6B22004500450045004500450045004500450045002200EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF0000450045004500450045004500450045004500450045002400610838C6FFFFFFFFD2940100450045004500450045002400610838C6FFFFFFFFD29401004500450045004500450045004500450045004500000099CEFFFFFFFFBEF7DEF795AD01004500010017BEDEF7DEF7FFFFFFFF17BE0000DBDEFFFFFFFF821000000000",
    "0000000000000000010000007DEFFFFFFFFFC31824004500450023000C63FFFFFFFFEF7B2200450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450024004108F7BDFFFFFFFF75AD210045004500450045002300EB5AFFFFFFFF918C020045002300E739FFFFFFFFDAD6000024004108F7BDFFFFFFFF75AD21004500450045004500450002000100240045004500450024008210FFFFFFFFFFFFAE7322004500450045004500450045004500450045002200EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF0000450045004500450045004500450045004500450024004108F7BDFFFFFFFF75AD2100450045004500450045002400",
    "4108F7BDFFFFFFFF75AD2100450045004500450045004500450045004500450045000100F39CFFFFFFFFFFFFFFFF8E732200450022001084FFFFFFFFFFFFFFFF718C0000F39CFFFFFFFFAA522300450045004500450045004500010058C6FFFFFFFF8A5223004500450023000C63FFFFFFFFEF7B220045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450044006108F7BDFFFFFFFFB6B5200000000000000000000000220062087DEFDEF77DEF82102400000034A5FFFFFFFFCF7B22006108F7BDFFFFFFFFB6B52000000000000000000000000000D29475ADA63101002400240001008D6BFFFFFFFFFFFF484223004500450045004500450045004500000000000000",
    "EF7BFFFFFFFFFFFF00000000000045004500000000000000EF7BFFFFFFFFFFFF000000000000450045004500450045004500450044006108F7BDFFFFFFFFB6B520000000000000000000000022006108F7BDFFFFFFFFB6B5200000000000000000000000220045004500450045004500450023004C63FFFFFFFFFFFFFFFFA6312400450023002842FFFFFFFFFFFFFFFFEB5A22008631FFFFFFFFBAD6610823002400230001000100220022003084FFFFFFFF17BE000024002400000034A5FFFFFFFFEF7B22004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500010038C6FFFFFFFFFFFFDEF7DEF7FFFFFFFFFFFFFFFFFFFFEF7B01006D6BFFFFFFFF18C64529EF7B",
    "FFFFFFFFFBDE6208010038C6FFFFFFFFFFFFDEF7DEF7FFFFFFFFFFFFFFFFFFFFB294DEF7DEF7DEF7B2944529C3180F7CDEF7FFFFFFFFBAD6210045004500450045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF0000450045004500450045004500010038C6FFFFFFFFFFFFDEF7DEF7FFFFFFFFFFFFFFFFFFFFEF7B38C6FFFFFFFFFFFFDEF7DEF7FFFFFFFFFFFFFFFFFFFFEF7B2200450045004500450045002400A631FFFF",
    "FFFFFFFF9DEF20004500450044006108DEF7FFFFFFFFFFFF4529240002001084FFFFFFFFDAD6694A6108C73975AD38C68E6B22000421BEF7FFFFFFFFB6B52421A63118C6FFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7B220001003084FFFFFFFFFFFFFFFFFFFFFBDE452924000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7BAA52BEF7FFFFFFFFFFFFFFFFFFFFFFFFFFFF5CE76529240045004500450045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF0000",
    "4500450045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7BFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7B22004500450045004500450045002100BEF7FFFFFFFFD6B50100450045004500010038C6FFFFFFFF9DEF00004500450001008E73BEF7FFFFFFFFFFFFFFFFFFFFDEF7518C220022004C63FFFFFFFFFFFFFFFFFFFFFFFF9DEFFFFFFFFFEF7B2200450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045000000000000000000000000000000000000000000000022004500450002002421108495AD34A5AA5241002400450045000000000000000000000000000000000000000000000022002300E318B294",
    "5CE7DEF7DEF7DEF75CE7718CA21023004500450045004500450045004500450045000000000000000000000000000000000000000000450045000000000000000000000000000000000000000000450045004500450045004500450045000000000000000000000000000000000000000000000022000000000000000000000000000000000000000000000022004500450045004500450045004500450000000000000001004500450045004500450001000000000000004500450045004500220082100B5B34A5",
    "B6B514A5EB5A82100200450045000200494A99CEDEF7DEF7BEF795AD863155AD17BEAA522300450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500240022000100010023004500450045004500450045004500450045004500450045004500450045004500450024000200000000000000000000000200240045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500440023000100010001002300240045004500450045002300000000000000000001002400010001002300450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450046004600460047004700470047004700470047004700460046004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500460046004700CA008F015302F702F702F702F702F702F70253028F01CA004700460046004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500460047004D017402F702F933DA64DC951C9EBDC6BDC61C9EDC95DA64B82BF70274022C014700460045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004600A9005302F702",
    "59441C9E7DB67DB67DB67DB67DB67DB67DB67DB67DB67DB61C9E5944F7025302A90046004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500460046002C01D602370B5B7D1CA61CA61CA61CA61CA61CA61CA61CA61CA61CA61CA61CA61CA61CA65B7D370BD6022C01460046004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500460046004D01F702370B5B7DDC95DC95DC95DC95DC95DC95DC95DC95DC95DC95DC95DC95DC95DC95DC95DC955B7D370BF7024D014600",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450024000200020024004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450046002C01F702F70259447B857B857B857B857B857B857B857B857B857B857B857B857B857B857B857B857B857B855944F702F7022C01460045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450024006529718C718C6529240045004500450045004500450045004500450045004500",
    "2400020001002200450045004500450045004500450045002400020001002200450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450046008900D602F702F702FA643B753B753B753B753B753B753B753B753B753B753B753B753B753B753B753B753B753B75FA64F702F702D602A9004600450045004500450045004500450045004500450045004500450045002200000000000000",
    "2400450045004500450045004500450045004500450045004500450045004500450045004500450045004500450000003CE7FFFFFFFF1BDF0000450045004500450045004500450045004500230001002421918CF7BDEF7B220045004500450045004500230001002421918CF7BDEF7B22004500450045004500450022000000000000000000000023004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500460047005302F702F702F702FA64FA64DC95",
    "7FE7FFFFFFFFDFF77DB61B6DFA64FA64FA64FA64FA64FA64FA64FA64FA64FA64FA64F702F702F70253024700460045004500450045004500450045004500450045004500450022008D6BFFFFFFFFFFFFA63124004500450045004500450045004500450045004500450045004500450045004500450045004500450045000000DAD6FFFFFFFFBAD600004500450045004500450045004500450023008A5217BEDEF7FFFFFFFFFFFF0000450045004500450023008A5217BEDEF7FFFFFFFFFFFF0000450045004500",
    "24004100AE731BDFDEF7DEF7DEF799CE894A0100450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450046004D01F702F702F702F7029A54DC95FFFFFFFFFFFFFFFFFFFFFFFF5EDF9A549A549A549A549A543B751CA61CA61A6D9A547A54F702F702F702F7024D014600450045004500450045004500450045004500450045004500010038C6BEF7FFFFFFFF50842200450045004500450045004500450045004500450045004500240022000100020023004500",
    "4500450045002400C3186D6B6D6BC3182400450045004500450045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFF000045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFF0000450045002400C31879CEFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7B22004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500460047007402F702F702F702F7025944FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF3CA6594459449A54DEC6FFFFFFFFFFFFFFFF5DAED82BF702F702F702",
    "F70274024700460045004500450045004500450045004500450045002400E318FFFFFFFF7DEFBEF7FBDE00004500450045004500450045004500450045004500440001006529308495ADD294073A0000000000000000220024002200220024004500450045004500450045004500450045002300C7394C6395ADFFFFFFFFFFFF000045004500450045002300C7394C6395ADFFFFFFFFFFFF00004500450022003084FFFFFFFF38C6073AE318D294FFFFFFFFDEF70421240045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004600CA00F702F702F702F702F7023B75FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFECEF933F9333CA6FFFFFFFFFFFFFFFFFFFFFFFF9B85F702F702F702F702F702CA004600450045004500450045004500450045004500450022008D6BFFFFFFFF95ADFFFFFFFFA6312400450045004500450045004500450045002400620875ADFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7B0100000000002200450045004500450045004500450045004500450023000100",
    "EF7BFFFFFFFFFFFF00004500450045004500450023000100EF7BFFFFFFFFFFFF000045004500450042084D6B2C630000230024000000F7BDFFFFFFFF4C63230045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450046008F01F702F702F702F702F7027B85FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFB823B823FFFFFFFFFFFFFFFFFFFFFFFFFFFFBFF7F702F702F702F702F7028F0146004500450045004500450045004500450045004500010038C6DEF7BEF7894AFFFF",
    "FFFF50842200450045004500450045004500450045000100B6B5FFFFFFFF99CEC739894A7DEFDEF7DEF796B55084A731EF7BFFFFFFFFEF7B220045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF000045004500450045002200230045004500450022001084FFFFFFFFCF7B22004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450047005302F702F702F702F702F7023B75FFFFFFFF",
    "FFFFFFFFFFFFFFFFFFFFFFFFDEC658137A54FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF702F702F702F702F702530247004500450045004500450045004500450045002400E318FFFFFFFFB6B541089DEFDEF7FBDE000045004500450045004500450045002400C739FFFFFFFFDEF7E31823002200AA52FFFFFFFFF39C00000200EF7BFFFFFFFFEF7B220045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF0000450045004500",
    "4500450045004500450045000100B294FFFFFFFFCB5A23004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004700F702F702F702F702F702F7029823FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF3B7517035813FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF702F702F702F702F702F702470045004500450045004500450045004500450022008D6BFFFFFFFF4C63000095ADFFFFFFFFA631240045004500450045004500450022008D6BFFFFFFFF79CE0000450044004108FFFF",
    "FFFF7DEF00002200EF7BFFFFFFFFEF7B220045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF0000450045004500450045004500450024002200C3185CE7DEF77DEF82102400450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004700F702F702F702F702F702F702F702DA64FFFFFFFFFFFFFFFFFFFFFFFF1ED7F702F702F7021CA6FFFFFFFFFFFFFFFFFFFFFFFFDC95F702F702F702",
    "F702F702F7024700450045004500450045004500450045004500010038C6FFFFFFFFE31822000C63FFFFFFFF5084220045004500450045004500450022006D6BFFFFFFFF79CE0000450044006108FFFFFFFFBEF700002200EF7BFFFFFFFFEF7B220045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF0000450045004500450045004500000004218E735CE7DEF7BEF7694A2300450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004800F702F702F702F702F702F702F702F7029A541ED7FFFFFFFF7EE77B85F702F702F702F70258131ED7FFFFFFFFFFFFFFFFBDC65813F702F702F702F702F702F7024700450045004500450045004500450045002400E318FFFFFFFF58C600002400A210FFFFFFFFDBDE000022000100010001000100010002006529FFFFFFFFDEF7031923002200AA52FFFFFFFF38C601002200EF7BFFFFFFFFEF7B220022000100010001000100010022004500450045002200",
    "EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF0000450045004500450045000000FFFFFFFFFFFFFFFF7DEF073A01004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004800F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F7029A547B857B859A54F702F702F702F702F702F702F702F70247004500450045004500450045004500450022008D6BFFFFFFFFCF7B000000000000",
    "38C6FFFFFFFFA631EF7BF7BDF7BDF7BDF7BDF7BDEF7B0000B294FFFFFFFFBAD6A631694A7DEFFFFFFFFF8A5223002200EF7BFFFFFFFFEF7B0100EF7BF7BDF7BDF7BDF7BDF7BDEF7B2200450045002200EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF00004500450045004500450023002C6395ADBAD6DEF7DEF7DEF7718C210045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004700F702F702F702F702F702F702F702F702F702",
    "F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702470045004500450045004500450045004500010018C6FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF3084FFFFFFFFFFFFFFFFFFFFFFFFFFFF000082103CE7FFFFFFFFFFFFFFFFFFFFDEF7AE73010045002200EF7BFFFFFFFFEF7B0000FFFFFFFFFFFFFFFFFFFFFFFFFFFF0000450045002200EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF0000450045004500",
    "4500450045002300010000000421DAD6FFFFFFFF139D01004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004700F702F702F702F702F702F702F702F70298233B757B857B857B853944F702F702F702F702F702581339447B857B853944F702F702F702F702F702F702F702F702470045004500450045004500450045002400E318FFFFFFFFDEF77DEF7DEF7DEF7DEF7DEFFFFFFFFFDBDE0000000000000000000000000000610879CEDEF7DEF7B294D6B5D6B5EF7B0319",
    "2200450045002200EF7BFFFFFFFFEF7B220000000000000000000000000000004500450045002200EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF00004500450045004500450045004500450045002300073AFFFFFFFFFFFF8631240045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450047005302F702F702F702F702F702F933BDC6FFFFFFFFFFFFFFFFFFFFFFFF7EE7DA64F7025813DC95BFF7FFFFFFFFFFFFFFFFBFF77B855813F702F702",
    "F702F70253024700450045004500450045004500450022008D6BFFFFFFFF929400000000000000000000FBDEFFFFFFFFA6312400450045004500450023002C63FFFFFFFF38C6000001000100220024004500450045002200EF7BFFFFFFFFEF7B220045004500450045004500450045004500450045002200EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF000045004500450045004500450045004500450045002000FFFFFFFFFFFF8D6B22004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450047008F01F702F702F702F702DA64FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF1CA658131ED7FFFFFFFFFFFFFFFFFFFFFFFFFFFF1ED75813F702F702F7028F0146004500450045004500450045004500010018C6FFFFFFFF2842230045004500450002009294FFFFFFFF30842200450045004500450023000C63FFFFFFFF9DEF284220000000000000000000010024002200EF7BFFFFFFFFEF7B220045004500450045004500450045004500450045002200",
    "EF7BFFFFFFFFFFFF00004500450045004500450045002200EF7BFFFFFFFFFFFF000045004500450002000100240045004500450024008210FFFFFFFFFFFFAE7322004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004600CB00F702F702F7029A54FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF1CA6F933BFF7FFFFFFFFFFFFFFFFFFFFFFFFFFFF1ED7F702F702F702CB0046004500450045004500450045002400E318FFFFFFFF7DEF21004500450045004500",
    "23002842FFFFFFFFDBDE0000450045004500450045004208D6B5FFFFFFFFFFFFFFFFFFFFFFFFFFFFBEF7F7BDA6310100EF7BFFFFFFFFEF7B220045004500450045004500450045004500000000000000EF7BFFFFFFFFFFFF00000000000045004500000000000000EF7BFFFFFFFFFFFF0000000000000100D29475ADA63101002400240001008D6BFFFFFFFFFFFF48422300450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500460047007402F7025813BFF7FFFFFFFFFFFFFFFF",
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFDA647B85FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7B85F70274024700460045004500450045004500450022006D6BFFFFFFFF34A501004500450045004500450021007DEFFFFFFFFF8631240045004500450045000100CE73DEF7DEF75DEF9DEF9DEFBEF7DEF7FFFFFFFFBEF70421EF7BFFFFFFFFEF7B220045004500450045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF6529",
    "DEF7DEF7DEF7B2944529C3180F7CDEF7FFFFFFFFBAD621004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450046004E01F7021CA6FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7EE75813BFF7FFFFFFFFFFFFFFFFFFFFFFFFFFFF7EE7F7020C0146004500450045004500450045004500010017BEFFFFFFFFCB5A230045004500450045004500010054A5FFFFFFFF3084220045004500450022008E73FFFFFFFF8D6B00000000000000006108",
    "58C6FFFFFFFF8D6BE839F39CF39C083A230045004500450045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF0000AA52BEF7FFFFFFFFFFFFFFFFFFFFFFFFFFFF5CE765292400450045004500450045004500450045004500450065008600C708E808E8080809080908090809E808E808C708A6006600450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004600470053027DB6FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFDA641CA6FFFFFFFFFFFFFFFFFFFFFFFFFFFF1ED7",
    "53024700460045004500450045004500450045004500010000000000230045004500450045004500450045000100000000002200450045004500450000003CE7FFFFFFFF4108440045004500450002003084FFFFFFFF8E730200010001002300450045004500450045004500450045004500000000000000000000000000000000000000000045004500000000000000000000000000000000000000000045002300E318B2945CE7DEF7DEF7DEF75CE7718CA2102300450045004500450045004500450045004500",
    "A608E8080809080908090809080908090809080908110811091129112911291129110809C708860045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004600AA00771BBFF7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF1CA69A54FFFFFFFFFFFFFFFFFFFFFFFFBFF7771B89004600450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450000009DEFFFFFFFFF863122002400240022006108DAD6DEF7DEF745292400450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450024000200000000000000000000000200240045004500450045004500450045004500A708080908090809080908090809E8080809080908090809080908090809080908110911291129114911491149110809A7086500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500460047000C01F933BFF7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFBDC69823FFFFFFFFFFFFFFFFFFFFBFF7F9330C0147004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045000200F39CFFFFFFFF5CE74C63C318A2100C631BDFFFFFFFFFEF7B02004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500A6000809080908090809080908090809E80808090809E808E808E808E808080908090809080909090909090929112911",
    "491149116A116A112911C708650045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500460047004E01F933BFF7FFFFFFFF",
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7EE7F702FFFFFFFFFFFFFFFFBFF7F9334E0147004600450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004400620834A5FFFFFFFFFFFFFFFFFFFFFFFFFFFF3CE7CB5A01004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045006500080908090809080908090809E808E808E808E808E808E808E808E8080809E808E8080809080908090809080908090809090929112911291149114A118A118B114911C708450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500460047000C01771B1ED7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFBDC69823FFFFFFFFFFFF1ED7771B0C0147004600450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450044000100E3186D6B54A5D6B595AD3084C631",
    "00002300450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500860009090809080908090809E808E808E808E808E808E808E808E808E808E808E808E808E808E808E808E808E808080908090809080909090809291129112911491149116A11AB11AB194911C70866004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450046004700AA005302DA647EE7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFBDC63944FFFF7EE7DA645302AA00470046004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450024002200010001000100220024004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500A60009090809080908090809E808E808",
    "E808E808E808E808E808E808E808E808E808E808E808E808E808E808E808E808E808E8080809E808080908090809080929112911291129114A116A118B11AB19CC198A11C70865004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004600460047000C017402DA647DB6BFF7FFFFFFFFFFFFFFFFFFFFFFFF7B859823DA6474020C0147004600460045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500A6002911080908090809E808E808E808E808E808E808E808E808E808E808E808E808E808E808E808E808E808E808E808E808E808E808E808E808E808E8080809080908090809080929112911491149114A116A118B11EC19EC196A11A6004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500460046004700CB008F01530298233944394439443944982353028F01CB00470046004600450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500A6002909290908090809080908090809E808E808E808E808E808E808E808E808E808E808E808E808E808E808E808E808E708E808E808E808E808E808",
    "E8080809E808E80808090809080908090809290929112911491149116A118A11CC192D1AED19491165004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4600460047004700470048004800480048004700470047004600460045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450086002911080908090809080908090809E808E808E808E808E808E808E808E808E808E808E708E708E708E708E808E808E808E808E808E708E708E708E708E808E808E808E808E808E808E8080809080908090809290929092909291149114A116A11AB110D1A6F22EC1908094500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450065002911290909090809080908090809E808E808E808E808E808E808E808E808E808E708E708E708E708E708E708C708C708E708C708C708E708E708E808E808E808E708E708E708E708E808E808E808E8080809080908090809080909092909291149114A116A11",
    "8A11EC194E228F22AC19C7084500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450065008600C708E80808110811081108110809E708A60886004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450065002909290929090809080908090809E808E808E808E808E808",
    "E808E808E808E808E708E708C708E708C708C708C708C708C708E708E708E808C708C708C708C708C708E808E808E808E808E708E708E808E808E808E808E708E808080908090809080909092909291149114A116A118B110D1AB0228F228B118600450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045008600080929112911291129112811291129112911291149114911491149110809A70865004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500E808291129092909080908090809E808E808E808E808E808E808E708E808E708E708E708E708C708C708C708E708E708E708E708C708C708C708C708C708E708E808E808C708C708C708E708E808E808E708C708C708E8080809E708E808E8080809080908090809080929092909290949114A116A11AB114E22F12A6F226A11650045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500A60028112911291128112811281128112811281128112811291129112911491149114911691169112811A6084500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500C7084911290908092909080908090809E808E808E808E808E808E708E708E708C708E708C708E708E708E708E708E708C708C708C708C708C708C708E708E708C708C708",
    "C708C708E808E808C708C708C708E708E808E708C708E708E808C708C708E808E808E808E808E80808090809080929092909491149114A116A11EC19B022322B6F222909450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500650008094911291128112811281128112811281128112811281128112811281129112911491149114911491169118A116A110809A6084500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500860049114911290929090809080908090809E808E8080809E808E808E708E708E708E708E708E708E708E708E708C708C708C708C708C708E708E708E708C708C708C708C708E808E808C708C708C708E708E808C708A700C708E808C708C708E808E808C708C708E708E808E808E808E8080809080908090809290929092909491149116A110D1A112B322B6F22290945004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500A608491129112911",
    "2911291129112811281128110809281108092811281128112811281129112911291129114911491149116A118A118A116A11E70865004500450045004500450045004500450045004500450045004500450045004500450045004909491129092909080908090809080908090809E808E808E808E808E808E808E808E808E808E708C708C708C708C708C708C708E708E808E708C708C708C708C708E808E808C708A700C708E808E808C708A700C7080809E808A700E808E808A700C708E808E808C708C708E808",
    "E808E708E708E8080809E808080908090809290929092909491149118A114E1A322B532B8F22491165004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500E8084911491129112911291128092809080908090809080908090809080908090809080908092809280929112911291149114911491149116A118A11AA11AB110809650045004500450045004500450045004500450045004500450045004500450008094A1149092909290908090809080908090809E808E808E808E808E808E808",
    "E808E708E808E708C708C708C708C708C708E708E708E708C708C708A700C708E708E808C708A700A700C708E808C708A700A700E808E808C708A600E708E708A700E808E808A700C708E808E808C708C708E808E808C708E8080809E808E8080809080908090809080929092909290949116A118F22322B732BD022AB118600450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500650028094911491149112911280928092809280908090809080908090809080908090809080908090809080908090809280928092911",
    "291149114911491169116A118A11AB11CB1908096500450045004500450045004500450045004500450045004500A7006A11491149092909290908090809080908090809E808E808E808E808E808E808E708E708E708C708C708C708C708C708E708E708E708C708C708C708C708E808E808C708A700A700E708E808C708A600C708E808C708A600A700E808C7086600E808A700A600E808E808A600C708E808C708A600E708E808C708C708E808E808C708E8080809E808E8080809080908090809290929094909",
    "AB114E1A322B732B11234E1A8A11860045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500650029114911491149112911280928092809080908090809080908090809080908090809080908090809080908090809080908092809280929112911291149114911491149116A118A11CB19CB192809650045004500450045004500450045004500450065006A116A1149094909290908090809080908090809E808E808E808E808E808E808E708E708E708C708C708C708C708C708E708E808C708C708A700A700C708E708E708C708A600A700E708",
    "E708A700A600C708E808C7088600C708E808C70886004909A600A700E808C7088600E708E808A600A7000809C708A600E808E808A600E708E808C708C708E808E708C708E808E808E808080908090809080929094909AB116E1AD022522B732B11232D1A29096500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500650049114911491129112911291128092809080908090809080908090809080908090809080908090809080908090809080908090809080908092809280929112911291149114911491169118A11AA11CB19EC192909650045004500450045004500",
    "4500450029096A11491149092909290908090809080908090809E808E808E808E808E808E808E708E708C708C708C708C708E708E708E708C708C708C700A700C708E708E808C708A600A700E708E808C7008600C700E808C7088600C700E808C7088600C708C700A7000809A7008600E808E7088600C708E808A600A7000809A700A6000809C700A600E808C708A700E808E808A700E808E808C708E808E808E80808090809080908094909AB11EC118F1AF122522B522BD022EC11E80845004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450025002500250025002500250045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045006500691149114911491129112911280928092809",
    "2809280908090809080908090809080908090809080908090809080908090809080908090809080908092809080928092911291129114911491169116A118A11AB11EC190C1A290945004500450045004500C7008A116A1149094909290929092909080908090809E808E808E808E808E808E808E708E708E708E708C708C708C708C708E708E708C708C700A700A700C708E708C708A700A600C708E808C708A600A700E808E7088600A600E808C7088600C700E8084500E808C7008600E808C7088600C708E808",
    "8600C70008098600A70008098600C70008098600C7000809A600C7080809A600C708E808A700E808E808C708E808E808E7080809E808080929096A110D1A2D1A8F1AF122522B522BD022EC110809650045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500C9008E015202F702F702F702F702F702F70252028E01A8002500450045004500450045004500",
    "4500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450065006911691149114911291129092809280928092809280908090809080908090809080908090809080908090809080908090809080908090809080908090809080929090809080928092809290929114911491169116A118A11CB112D1A2D1A08094500450065008A118A116A11490929092909290908090809080908090809E808E808E808E808E808E808E708",
    "E708E708C708C708E708E708E708E708C708C700A700C700E708E808C708A600A700E708E708A7008600C708E808C7008600C708E808A600A6000809C7088600E7088600C70808098600A7000809A600A70008098600A70008098600C70008098600E708E8088600E808E80886000809C708A6000809A600C7000809A600E708E808C700E808C7080809E808E8082909AB11CC110D1A2D1A8F1AF02232235223F0222D1A490986004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045002C017302F702F933DA64DC951C9EBDC6BDC61C9EDC95DA64B82BF70273020B01250045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450029096A1169114911491149112909280928092809280908090809080908090809080908090809080908090809080908090809080908090809080908090809",
    "08090809E70808090809080928092809290949114911491169116A118A11AB11EC198F22CB114500E708AB116A114909490949092909290908090809080908090809E808E808E8080809E808E808E708E708E708E708C708C708E708E708E708C708C700A700C708E708E708C700A600A700E808E808A600A600E708E808A6008600E808E7088600C7000809A700E808C7008600E808C7086500E708C7086500E808C7086500E808C70086000809C700A60008098600C700080965000809E70886002909A600C708",
    "290965000809E70886004909A6002909A600E7082909C700E80808098B11CC11EC11EC112D1A4E1AAF223223932B732B8F1A4909450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450087005202F70259441C9E7DB67DB67DB67DB67DB67DB67DB67DB67DB67DB61C9E5944F7025202870045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500C7088A116A114911491149112909290928092809280908090809080908090809080908090809080908090809E808E8080809E808E808E808E808E808E808E80808090809E8080809080908090809080928092809290949114911691169118A11AB11EC196E224D1A2D1A8A116A114909490929092909290908090809080908090809E808E808E8080809E808E708E708E708E708C708C708C708E708E708E708E708C700A700C700E708E808C708A600C700E808E708A600",
    "A600E808E708A600A600E808C7008600C700E80886000809A600A6000809A600A6000809A700A6000809A600A60008098600C700E8086500E708C70886000809A600C70008096600E808C708860008098600E708E8088600080945000809A600E808E708A6000809C700E708E808E708AB118A11CC11EC11EC110D1A4E1A8F1A112332231123AF1AEC114909860045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045000B01D602370B5B7D",
    "1CA61CA61CA61CA61CA61CA61CA61CA61CA61CA61CA61CA61CA61CA65B7D370BD6020B012500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450008098A116A1169114911491149112909280928092809080908090809080908090809080908090809E8080809E8080809E8080809E808E808E708E708E708E708E808080908094911E808E808080908090809080928092809290929094911491149116A118A11AB11AF22",
    "522BF022CC11490949092909290928090809080908090809E808E808E808E8082809E808E808E708E708E708C708C708C708C708E708E708E708C708C700C700E708E808C708A600A700E708E708A6008600E708E808A600A600E808C7006600E708E808660049098600C70008098600A700E8088600C700E8086500C708C70865000809A700A60008096500C708C708650008098600C700E708650008098600C708E708660008098600490986000809A600C700080986000809A600C708E808A600080908094909",
    "CB11AB11CB11CB11EC11EC112D1A6E1AB01A11233123F1228F1ACC114909A60045004500450045004500450045004500450045004500450045004500450045004500450045004C01F702370B5B7DDC95DC95DC95DC95DC95DC95DC95DC95DC95DC95DC95DC95DC95DC95DC95DC955B7D370BF7022C012500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045002809AA118A1169114911491129092909280928092809080908090809",
    "080908090809E80808090809E8080809E808E808E808E708E708E708E708E708E708E808E808080908090809A700E80808090809080908090809080908090809280928092809491149114911EC11AF22AF22F022512BF0228A112909290928092809080908090809E808E808E808E8080809E808E808E808E708E708E708C708C708C708E708E708E708E708C700A700C700E708E708A700A600C708E808C7008600C708E808A6008600E808C7086600C700E808860029098600C700E8088600C700E8086500C700",
    "C7086500E808A700860008096600C700C708650008098600C700E708650008098600C708C708660008096500E808A700A600A600E808C700A6000809660028098600E808C708A600080986000809A6000809E8086A11AB116A11CB118B11CB11CB11CC11EC110D1A4E1AAF1AF0221123F122AF1A0D1A8B11450045004500450045004500450045004500450045004500450045000B01F702F70259447B857B857B857B857B857B857B857B857B857B857B857B857B857B857B857B857B857B855944F702F7020B01",
    "2500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500E708AA118A116A114911C508210000000000000000004200C600080908090809080908090809C700630042006400E708E808E708E708E708E708E708E808E808E8080809E808E708E708E70808090809E808E708E708E8080809080908090809280908092809280969116E1A8E1A6E1A6E1A8E22D022722B8F1AE608620042006300C600080908090809E808E808C708E808E808E808",
    "E708E808E708E808E708C708C708C708E708E808E708C700A700C708E808E708A600A600E708E708A600A600E808C7088600C700E8088600A7000809A6000809A600A60008098600A70008096500C700E7086500E808A600A60008096500E708C700860008094500E708A600A600E808450008098600C708C700860008094500080945000809650008096500E808A600C700E708860008096500290966000809A600C708E70886006A1108098B114909CB116A11AB11AB11AB11CB11CB11EC11EC112D1A6E1AAF1A",
    "45004500450045004500450045004500450045004500450045008700D602F702F702FA643B753B753B753B753B755B7D9DBEBFEFFFFFFFFFBFEF9DBE5B7D3B753B753B753B753B75FA64F702F702D602870045004500450045004500450045004500450045004500450000000000000000000000000000000000000022004500A700AB118A116A1128094100CB5A79CEBEF7DEF7DEF7BEF7F7BDC7396300080908090809A50021002421918CF7BDEF7B6300E708E708E70800000000000000000000000000000000",
    "0000000000006300E708E708C60042000000000000002000850008090809080928090C128F1A4D1A4D1A4D1A4D1A4D1A2609A2102C6354A596B5518C86314100E700E808E808E808C708E808E708E708E808E808E808E808E808E708E708C708C708C708E808E808C700A600C700E808E808A600A700E808E708A600C7000809A7008600E808E7086600E708C7088600C7086500E808C70086000809A600A60008094500E708A700860008096500E708A700860008094500E8088600A700C7086500080945000809",
    "8600C700C7008600E8086500A600A600C7086600080945002809450008096600E708A600C700C708A600E8086600080965000809A6004909C700AB1149098A116A096A118A116A11AB118A11CB11AB1145004500450045004500450045004500450045004500450045005202F702F702F702FA64FA64FA64FA64FA647B859FEFFFFFFFFFFFFFFFFFFFFFFFFF9FEF7B85FA64FA64FA64FA64FA64F702F702F7023302250045004500450045004500450045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFFDEF7",
    "BEF7BAD66D6B41008A09AA116A112809E31859CEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF108463000809A5008A5217BEDEF7FFFFFFFFFFFF0000E708E7080000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7B6400C600042175ADBEF7DEF7DEF7FBDE0C6342000809080949118F1A4E1A4D1A2D1A2D1A0D126208CF7BBEF7FFFFFFFFFFFFFFFFFFFFB6B58208C708E808E8082809E808E808E808E808E808E708E708E708E808E808E808E708C708C700C7080809E808C700A700E7080809C700A600",
    "E708E808A600A7000809A700A6000809C7008600080945000809A600A60008096600C708E70865000809A600A700E808450008098600C700C708650008094500E808A600A700C70065000809450008096500C708A600A600C700C7006500E7088600A700A700A600C7006600E80865000809450008094500080945000809650008098600E8088600E708A700C70849094909490949098A114909AB1149096A094500450045004500450045004500450045004500450045002C01F702F702F702F7029A549A549A54",
    "9A549A545EE7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF5EE79A549A549A549A547A54F702F702F702F7024C0125004500450045004500450045004500450045000000FFFFFFFFFFFFDAD6DBDEFBDE7DEFFFFFFFFFFFFF38C6820869094909A210DAD6FFFFFFFF5CE78E73E318E318EF7B9DEFDEF75CE7A310E7000000FFFFFFFFFFFFFFFFFFFFFFFF0000E708E7086300EF7BF7BDF7BDF7BDF7BDF7BDF7BD38C6FFFFFFFFBEF78631A50024215CE7FFFFFFFF9DEFDEF7FFFFFFFF518C420008096E1A6E1A4D1A2D1A",
    "0C1A0C1AC40834A5FFFFFFFFFBDECE73AE731BDFFFFFFFFF54A5420008090809080908090809080908090809080908090809E808E808E808E80808090809E708C708C70808090809C700C700E8080809C700C7000809C700A6000809E708A6000809C700A6002909A60008098600C708E80865000809A600A7000809650008098600C700C70866000809450008098600C700C7008600E80845000809450008096500C7088600A700A700E8084500E8086500C7088600C700A600A600A600A600C7008600C7008600",
    "E7086500E8086500E8086500080965000809650008096500E808C7006A09A600AB110809AB1129094500450045004500450045004500450045004500450025007302F702F702F702F702D82B594459445944BC8DFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFBC8D594459445944D82BF702F702F702F702730225004500450045004500450045004500450045000000FFFFFFFFFFFF000000000000000065293CE7FFFFFFFF3084A4006200139DFFFFFFFF79CE82108400E700E70063004529CB52C739A500",
    "0809C600C7394C6395ADFFFFFFFFFFFF0000E708C708E7086300420042002100210021000000139DFFFFFFFF2C636300210017BEFFFFFFFF0F7C000086319DEFFFFFFFFFE73948098E1A2D1A2D1A0C120C120C12E508718CBAD675AD410806090609A2107DEFFFFFFFFF452928090809080908090809080908090809080908090809080908090809080908090809080908090809E808E808E8080809E808E70808090809C700E7080809A700C7000809A600E808E808A600E708860028098600E708E80886002809",
    "8600E808C700A6000809650008096500E808A600A600E7086500080945000809450008096500E7088600A700A600A600C700C7004500E8086500E7088600C7008600C7008600A700A600A700A600A700A600A600A600A700A600A7008600C7008600C7008600E7088600E7086500280986000809E808EC1145004500450045004500450045004500450045004500C800F702F702F702F702F702170BF933F933F9337DB6FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7DB6F933F933F933170BF702F702F702",
    "F702F702A8002500450045004500450045004500450045000000FFFFFFFFFFFF00004500450045002200EB5AFFFFFFFFDAD621002421FFFFFFFFBEF70421E6002809280908090809C600A500C600080908090809C6004200EF7BFFFFFFFFFFFF0000C708C708E708E7080809E708C708C70885000842FFFFFFFFB6B52100A500C739FFFFFFFF9AD62000C7004200EF7BFFFFFFFF95ADA4006E1A4D1A0C12EC11EC11EC11CC11C5086208A408CB090C120C12A408F39CFFFFFFFF2C634C1A722B0C12490929092909",
    "290929094909290949094909290949092909290929092909290928092809290928090809080908090809080908090809E708E8080809C7000809E708A7000809C7000809A6000809A700C70008098600280986000809A600C700E7088600080965000809650008096500E8088600C700A700A700C7008600E7088600E8086500080965008600C7008600C7008600C7008600A7008600A7008600C7008600C7008600C7006500E7086500E80845000809450008094500080945000809450008094500E80865004500",
    "450045004500450045004500450045004500450045008E01F702F702F702F702F702F702170B9823B823FECEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFECEB8239823170BF702F702F702F702F702F7028E012500450045004500450045004500450045000000FFFFFFFFFFFF000045004500450085008529FFFFFFFFDEF700000F7CFFFFFFFFB29462002809280928092809080908090809080908090809E708E7086300EF7BFFFFFFFFFFFF0000C708C708E708E7080809280949094A11410079CEDEF7",
    "BEF724214D1A06093084FFFFFFFFCE7347098F1A4D122421FFFFFFFFBEF76108F01AD01A8E1A8E1A6E1A4D1A0C12EC11CC11AB11CB11CB11CB11C5080F7CFFFFFFFFCF7B26091023F4333534D0228A1169096A098A11AB11AB11AB11AB11AB11AB11AB11AB11AB118A118A096A0969096909490949094909290928092909080908090809E80808090809E7080809C7000809C7000809C700E7080809A60028098600280986000809A600E808C700A700E80886000809650028096500280945002809450028094500",
    "280945000809450008094500E7088600C7008600C7008600C7008600A700A600A600A6008600A7008600C7006500E7084500E8084500080945000809450008094500E7086500C700A6008600C7004500450045004500450045004500450045004500450025005202F702F702F702F702F702F702F702170358139DB6FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF9DB658131703F702F702F702F702F702F702F70233022500450045004500450045004500450045000000FFFFFFFFFFFF0000450045008600",
    "89110842FFFFFFFF7DEF0000F7BDFFFFFFFF2842C5002809280928092809080908090809080908090809E808E8086300EF7BFFFFFFFFFFFF00004D1AAF1A112352239323932372234C128A52FFFFFFFF929405097223A400F7BDFFFFFFFFE7392C121123312321005CE7FFFFFFFFE739EF1AF42B152C152CF52B152C352C352C352C152C152C152C152CA811B294FFFFFFFF4D6B4C1A352C562C7634D83CF83C7634D42B932372233123101BCF1AAF1A8E1A4D1A2D120C120C12CB11CB11AB118A118A096A096909",
    "69096909490949092909490928092809080908090809E808E808E8080809C7000809C7000809A6000809A6000809A600E808C700E708C700C700E708C700E708A700E808A600E7088600E8088600E7088600E7088600E7088600450028094500280945002809450008094500E8086500E7086500C7008600A600A7008600C7006500E80845000809450008094500E8084500C70086008600C7006500E808450045004500450045004500450045004500450045002500F702F702F702F702F702F702F702F702F702",
    "F7023B75FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF3B75F702F702F702F702F702F702F702F702F702F7022500450045004500450045004500450045000000FFFFFFFFFFFF00004500440048094100139DFFFFFFFFB6B500003CE7FFFFFFFFC318070128092809C50063006300630063008400C700080908098A110609EF7BFFFFFFFFFFFF000052231023CF1AAF1AAF1AAF1A6E1A620017BEDEF7DEF70421EB11CF1A41001BDFFFFFFFFFE3180C126E1A6E1A620038C6FFFFFFFFEB5A0B12522352231123",
    "31235223522352235223932393239323B32B820099CEFFFFFFFFA6313123B32B7223512331233123F42B96341534F01A2D120D120D120C120C12EC11EC11CB11CB11AB11AB11AB118A118A118A096A096A094909490949094909290929092909080928090809080908090809E808E808E708E808E708E708E708C700E708C700E708C700E708C700E708C700C700C700C700E708C700E708A600E80886000809860008096500080945004909A600C700C700A600C7006500E8084500080945002909450028094500",
    "E8086500C70086008600C7006500E80845000809450008094500C70086008600C70045000809450045004500450045004500450045004500450045002500F702F702F702F702F702F702F702F702F702F7025813BFF7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFBFF75813F702F702F702F702F702F702F702F702F702F7022500450045004500450045004500450045000000FFFFFFFFFFFF000000004108C739B6B5FFFFFFFFDEF7E7390000DEF7FFFFFFFF000008092809C500694A10843084308430844C63C829",
    "CE1AB323B323C911EF7BFFFFFFFFFFFF00008E1A6E1A4E1A4D1A4D1A6E1A0C122421FFFFFFFFD6B562000C128E1A0000BEF7FFFFFFFF4108EB096E1A2D1A8300B6B5FFFFFFFFAE73A9113123F01A101B512311233123312372235223B323152C3023C631FFFFFFFFDBDE2100AB11CB110C12EC110C122D1A2D1A6E1A512B55343534AF1A6A0969096A096A096A096A096A096A096A096A09490969094909490949094909490949092909290929092909080908090809080908090809E8080809E8080809C7000809",
    "C7000809A6000809A6000809A6000809A700E808C700E708E708C700E8088600080986002809650029096500290965000809A6006500280945002909450028096500E7088600A700C7008600E70845002809450008094500E7088600A600A7006500E808450008094500E8084500A700A6006500E808450045004500450045004500450045004500450045002500F702F702F702F702F702F702F702F702F702F702F702F933BFF7FFFFFFFFFFFFFFFFFFFFFFFFBFF7F933F702F702F702F702F702F702F702F702",
    "F702F702F7022500450045004500450045004500450045000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF9DEFCA52A4080000DEF7FFFFFFFF0000080928090000BEF7FFFFFFFFFFFFFFFFFFFFEF7B8811CF1AAF1A4709EF7BFFFFFFFFFFFF00004D1A6E1A4D1A0C120C124D1A2709AE73FFFFFFFF4C634709EC116D1A0000FFFFFFFFFFFF00002D124D1AEC11C40875ADFFFFFFFFEF7BC408A400A400C408C400C4088811B323D42BF42B93236E1A4100D6B5FFFFFFFF4D6BC508CB118A118A11CB11AB11CB11",
    "CB110C12EC114D1AEF22353455343023CB110809290908092809280928092909290929092909280929092809290908090809280908090809080908090809E8080809E808E8080809E7080809C700E808C700E708E808C7000809A60028098600080986000809A700E708E708A60008098600290965002909650008098600E708A700450029096500E8088600C700C7008600080945002909450008096500C700A6008600E70845000809450008094500C700A6006500E708450008094500E80865008600C7004500",
    "45004500450045004500450045004500450045002500F702F702F702F702F702F702F702F702F702F702F702F702F933BDC6FFFFFFFFFFFFFFFFBDC6F933F702F702F702F702F702F702F702F702F702F702F702F7028600650045004500450045004500450045000000FFFFFFFFFFFF7DEF7DEFDEF7FFFFFFFF92948210C608691100005CE7FFFFFFFFA21007018A112709108417BE38C61BDFFFFFFFFFEF7B47098E1A6E1A2609EF7BFFFFFFFFFFFF00000C120C124D1A4D1A0C12CB118300D6B5FFFFFFFF2421",
    "CA09CB116E1A0000DEF7FFFFFFFF0000AB114D1A0C12060975ADFFFFFFFFCE73EF7BF7BDF7BDF7BDF7BDF7BDEF7BC9116E1A8A114909A400EB5AFFFFFFFF99CE4108AB110809AB11AA116909AB118A11CB118A11CB11AA110C12EC11AF22B32B5534D32B2D1A2809E7080809E7080809080908090809E7080809E8080809080908090809E80808090809E8080809E708E7080809C7000809C700E708E808C7000809A6000809A700E808E708A70008098600290986000809A600E708E708A6000809860029096500",
    "08098600C700C700860049098600C700C7008600080945002909450008098600A600C70065002809450008096500C700A6006500E808450028094500E70886008600E708450028094500E7088600450045004500450045004500450045004500450045002500F702F702F702F702F702F702F702F702F702F702F702F702F702F7029823394439449823F702F702F702F702F702F702F702F702F702F702F702F702F702F702AF1A922B722B312BEF226D1AAB114909C7000000FFFFFFFFFFFF000000009294FFFF",
    "FFFFB6B5410069116911420018C6FFFFFFFF0842AE1A152CB3238811A3004100EF7BFFFFFFFFEF7B26096E1A6E1A2609EF7BFFFFFFFFFFFF00004D1A0C122C120C120C124D1220009DEFDEF77DEF20002D122D126A0921005CE7FFFFFFFF61080C0A0C123123E508D6B5FFFFFFFF4D6BFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000809CB11A60024219DEFDEF7DEF7A6314709E708EB1149096A09AA1169096911AA11AB116911CB118A11EC118A112C1A4D1A922B1434353431238A110809A6000809C7000809E708",
    "C7000809C7000809C700E708E808C7000809C70008090809C7000809C700C7000809A6000809A700E708E808A600080986000809A700C70008098600290986000809A600C700E80886002909650028098600E708C700860028094500A700080965002909450008098600C700E70865002909450008096500C700C70065002809450008096500A700C700450028094500080965008600C700450028094500650045004500450045004500450045004500450045002500F702F702F702F702F702F702F702F702F702",
    "F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702CB116D1A6D1AAE22CF221023512B922BB22B0000FFFFFFFFFFFF00002C12C2107DEFFFFFFFFF694AE6086911C5003084FFFFFFFF718C47091023D01AAF1AAF1A4709EF7BFFFFFFFFEF7B27092D124D1A0609EF7BFFFFFFFFFFFF00002D122D1AEC112D12EB11CA090421FFFFFFFF58C662008A114D1A2909620079CEFFFFFFFF24212801AF1A142C820099CEFFFFFFFF694A0000000000000000",
    "000000000000C700CB11C600A210DAD6FFFFFFFF6D6B8400E708CB1128096909CB11E708AB11691169112909CB114909AA114909EC1169110C128A11AE1ACF22B32B3434F333CF220809280986002809A600E708E808A600080986000809C700C700080986000809080986002809A600E808E808A600280986000809C700A600080986002809A600E70808098600290986000809C700A7000809650029096500E808C70086002809450008090809650029096500E808A70086000809450029096500C700C7006500",
    "28094500080965008600E708450029094500E708860065000809450008094500A600C70045002909450045004500450045004500450045004500A7008A09D502F702F702F702F702F702F702F702F702F702F702F702F70239447B857B857B857B853944F702F702F702F702F702F702F702F702F702F702F702F7029402AB110C1A0C1A2C1A0C1A2D1A2C1A4D1A4D1A0000FFFFFFFFFFFF000075342A1AAE73FFFFFFFFDAD66208B32B71238631FFFFFFFF7DEFC3104D12AE1A8E1A8E1A2709EF7BFFFFFFFFEF7B",
    "26092D1A2D1A2709EF7BFFFFFFFFFFFF0000EB114D1A0C12CB114D1A68090842FFFFFFFF75ADA3002D126909F01A8300139DFFFFFFFF694A0709F42BB42B4100BEF7FFFFFFFF0319922331236909490929092809AA11A7008A09610838C6FFFFFFFFD294410049090809AB11C7088A118600CB11280969092809EB11C700CB114909AB110809EC112809EB114909EB112C1ACF223023D32B5534922B6E1AA60028098600E708E8088600290986002809A700C70008096500E70808098600290986000809E7088600",
    "290986000809C700A600280986002809A600C7000809650029098600E708E7088600290965000809A7008600280945002909C700450029096500C700C7006500290945000809860086000809450029096500C700C700450029094500E7088600650028094500080965008600E808450028094500A60049094500450045004500450045004909AE22D3331434522BB212F702F702F702F702F702F702F702F702F702F7027B857EE7FFFFFFFFFFFFFFFFFFFFFFFF7EE77B85F702F702F702F702F702F702F702F702",
    "F702F70210026A09CB110C1ACB110C1AEB11EB190C1AEB190000FFFFFFFFFFFF0000712B502B8208FBDEFFFFFFFFAE734B1AB734050995ADFFFFFFFFF7BD610868092C124C12A300EF7BFFFFFFFFEF7B26098E1A2D1A0609EF7BFFFFFFFFFFFF00004D1AAA114D1A0C12CB116809EB5AFFFFFFFFB29483004D1A0C12CB114709EB5AFFFFFFFF918C6709D4238D1AE739FFFFFFFFDAD66200EB11E7088A11490949094909490907016108F7BDFFFFFFFF75AD41004909C700CB11E7088A11280928098A1108094909",
    "8A11C708AB11490949094909AA110809AA116A11691169118A11AA11EB11AE1AAE22722BD3335534512B2D12E708A60008096500290965000809C700A6004909A600C7000809650029098600E700E808650029096500E808E7088600290965000809C7008600280965000809A700A600280945002809A600A6000809450029098600650029096500C700E708650049096500E708C700650029094500E808A60065002809450008098600860008094500280965008600E808450028094500A600E700450028096500",
    "4500450065008A11102314341434922BEF22AE222C120E12F702F702F702F702F702F702F702F702F9331ED7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF1ED7F933F702F702F702F702F702F702F702F7028C094909EB198A11CB11CB11AA110C1A8A112C1A0000FFFFFFFFFFFF0000512BD32BCE1ACA52FFFFFFFF9DEFC318F32B142C04193CE7FFFFFFFF1BDF2C63A210C3186D6B7DEFFFFFFFFFEF7B4709000000000000EF7BFFFFFFFFFFFF000000000000CB116D1A2C1A06096D6BFFFFFFFF5084E608",
    "AA112C12AA11EB0982087DEFDEF77DEF8210CF1A830034A5FFFFFFFFCF7B63008A11280908098A1128096A1107096208F7BDFFFFFFFFB6B5200000000000000000000000C5082809AB11E7088A0928096909E708CB118600CB11080969094909AB11C700EB110809AA112809CB1108094D1A0C1ACF22CF22512BF333F333922BAB1169096500E808A700860008094500C700E808650049096500E808C7008600290945000809A600A600280945002809A600A6000809450029098600A7000809450029098600A700",
    "0809450029096500A70049096500A700E808450029096500A700E808450029096500A700E708450029096500A700E700450029094500C700C700450029094500A700C700450029094500A600C70045008A11302B34341434712BEF228E226D1A4D1A2C1A0C128A11B402F702F702F702F702F702F702F933BFF7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFBFF7F933F702F702F702F702F702F70294022909AA096911EB11AA118A11EB1969110C1A69110000FFFFFFFFFFFF0000B734B7345534",
    "A30038C6FFFFFFFFF39C2609353430234529DBDEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF58C624190000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF0000EB118E1A2609CE73FFFFFFFF0F7C2609AB110C12CB112C1206096D6BFFFFFFFF18C64529EF7BFFFFFFFFFBDE8208280929098A114909690928094909420038C6FFFFFFFFFFFFDEF7DEF7FFFFFFFFFFFFFFFFFFFFEF7BA4008A11A6008A09280908098A11280949098A11E7086A1149090809AA11280949098A11080969116911691128092C1A8A11",
    "CF226D1A10231023F333F333512B6D1AC70029094500C7000809450029096500E700C7006500290945000809860086000809450028098600A6000809450028098600A6000809450028096500A6000809450029098600A60008094500A60008094500280986008600080945000809860065002809450008098600650028094500E8088600650028094500E708A600450029094500C700C70045002809450045003434712BEF22AE226D1A4D1A2C1A2C1A0C1AEB19AB09AA090F0AF702F702F702F702F7029823BFF7",
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFBFF79823F702F702F702F702F702AD0969094909CB116911AA11CB116911EB192809EF220000FFFFFFFFFFFF0000342C3534F42B2F23E739FFFFFFFFFFFFA63191235534CF2282086D6BDAD6DEF7FFFFFFFFBEF779CECB5A6200CE1A0000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF0000CF222C1A6709EF7BFFFFFFFFEF7B06096D1ACB118E1ACB114D1A63003084FFFFFFFFFFFFFFFFFFFFFBDE452968090809AA11280969116909",
    "28096A110000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7BA4004909C70069112909E808AA118600CB11C7086A112809280928098A09C700EB118600CB11080969096911691108098A118A118A118E1A4C1ACF226D1A712B922B3434922B6E1A2C128600E808860086000809450029094500E700C7006500290945000809A600860028094500080986008600080945002809860086002809450008098600650028094500A60029094500E808A600650049094500E700C700450049094500A700E708",
    "4500280965008600E808450008096500650008094500E8088600450029094500C700A700450008098E226D1A6D1A2C1A2C1A0C1AEB11EB19CA11EB116A11CB0969099302F702F702F702F702BDC6FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFBDC6F702F702F702F702740228094909AB092809EB1169118A11CB1149096D1AB63CD734000000000000142C3434F42B55347534B22B0000000000000B12CB11CB116D1A8911470941000000000000000000620047094D1A2C1A",
    "6D1A0000000000000000000000000000000000000000CF22EF22AF228E1A6711000000004709EC118E1A0C128E1AEB114D1A1023C9112421108495AD34A5AA524108E60069098A11E708EB118600EB11C700AA1108090000000000000000000000000000000000000000000063008A110709E80828096909C708AB116500EB116500CB11C7006A090809490928096909E708CB116500EC11A600AB11280969092809AA110809CB112C1A4D1A8E1A4C1AEF22CE22B22BD233D333B22B0C1ACB114500E700A6006500",
    "0809450028096500A600E708450049094500C700C700450049094500E708A700450049094500E700C700450029094500C70049094500A600E70845002809650086000809450008098600650029094500E708A600450049094500A600E700450028096500650008094500E8088600450029094500A7004909513BCF2A4D1AEB11EB19CB11EB118A11EB1169110C1A290989098B09D602F702F7023944FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF3944F702F702D602",
    "0901AA0949094911CB1108090C1A48090C1230231835183575347534142C3434342C7634B73C143472230C122D12CB114D1AEB110C128E1A69096D1AAA114D1ACB11CB114D12EB11AB112D120C12EB110C120C124D1AEB116E1A4D1A0C12CF1A0C1A8E1A8E1A6D1A8E1ACF1A4D1ACF226D1A6D1A8E1A4D1A6D1A6D1A2D1A6D1A4D1AF834762CD3230609420083008400AB1128098A1169094909690969112809AA11E708EB1186000C128600EB11A7008A11080949096909E708AA118600CB1165002909E7088A11",
    "A600CB114500EB114500CB1186008A11E708690928090809490949090809AA11A600EB116500EB11C7088A11480969092809AA11CB116D1A2C1A2C1AAE1A2C1ACE22EF22712BF333922B30238A11490965008600E708450028094500E708A600650028094500080986006500080945000809650065000809450008098600650028094500650029094500E700A700450029094500A600E708450028096500650008094500E8088600450029094500A600C700450028096500650008094500E708A600450028094500",
    "3143734B934B734B113BAE2AEB198A11AA11CB112809AB116A094909AD09F702F7027DB6FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7DB6F702F702AD096909E800EB110809CB118A11EB11D32B552C383DD73455345534142C75349634B63C71236E1AEC110C120C122C12EB11EC11EB110C1208098E1A8A092D1A8A092C1AEB11EB11CB114D1A8A11EB116D1A69092C120C12CB110C122C120C12EB118E1AAA118E1A2C1A4D1A4D1A6D1A2C1AAF1A0C128E1A8E1A",
    "2C12AE1A0C128E1A962CF834342CAA11CB114809CB1149096911AA110809EB11C700EB110809CB1128096911490949094909691108098A11C708CB11A600EB1165000C124500EB1145008A11A600CB116500EB114500EB114500CB1186008A11C7006909E70829094909E8086911080928096909C708AB118600EB116500EB11C7088A1128094909EB192C1A0C1A0B1A8E1A0C1A8E1A8E1AAE22502B912B1434512BCF226909C700C700450028094500E7088600650008094500080965008600E708450028096500",
    "8600E808450008096500450008096500650028094500E708A600450029094500A600E700450028096500650008094500E708A600450029094500A600E708450008096500450028094500A600C7004500AF32AF328F32F032F03A724B524B734BF03A0C220C1A69116A116A092809CE09F702BFF7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFBFF7F702EE01C800890949096911CB110C1A342C962C142C9634F73C7534342C9634B63C553410230C124D1AAA098E1A",
    "8A096D1A69094D1AAA114D1A6909F01A08096D1AAA110C128A092C12CB110C126A094D1AEB1169098E1A28092C122C1249094D1ACB11EB110C122C128A118E1AAA114D1A2C1AEB118E1A0C122C1A8E1A0C12AE1ACB11552C39359223AA118911EB114909CB118A116911CB110809CB1108098A1169112809CB11C700EB118600EB11C700AA1108096911490928098A11E7088A11E70869094500EB116500EB114500EB116500CB118600AA11A6008A09C7006909E7084909080908094909E8086911080928094909",
    "E7088A11C700CB118600EB116500EB11A6006911CB118A112C1AAA118E1ACB116D1A4C1A4D1ACE228E22712B912BD333502B6D1ACB11650028094500E7008600650008094500080965008600E7084500080965006500E7084500A60008094500E8086500450028094500C700A600450029094500A600E708450008096500450028094500C700A600450028094500650008094500C700A60045002809450045008F2A4D2ACF324D2AAF324D2AF0326E32F03A724B524B3243CF32AB116A0949098D013713BDC6FFFF",
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFBDC637138D0928096909E8080C1A6D1A552C762CD32B9223D32B383D55347534F73C55341023AB116D1AEB11EC112D12CB112C128A096D1A49096D1A49098E1A86006D1A8A11CB110C12EB1169094D1A8A11EB11CB11AA112C1249096D1A29090C124D1A08098E1A6909EC11EB11CB11CB114D1AAA112D12EB110C122C12EB118E1A2C126D1A552C5935CF1A6911EB114909EB116911AA118A116911CB110809CB11490969118A112809",
    "8A1128098A0949116909691128096911280969092809690908096909280929096911690908096909E7088A09C7088A09C7086A09C7086909E708490907092909080928092809080948090809480908092809280928096909E7086A09C700AA11A600AA11A60028098A09AA11CB110C1AEB110C1A0C1A2C1A2C1A6D1A2C1ACE228E22502B912B922B712B4D1A0C128600A700A700450028094500E7008600450008094500E708860045006A098600450008094500C700A600450028094500A600C700450028094500",
    "650008094500E700A6004500280945008600E7084500E70886004500280945008600E708450065000C228E2A2D2A6E2A2D2ACF320C2A8F32AF322D2ACF32D03AD03A734B524B6E324D224A097302DA647EE7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7EE7DA6453026B0928092801CB091023762C762CB223B2231023B22B59459634F73C75343023EC112C120C122C120C12CB112C1A8A094D1ACB11AA110C12CB110C128A112C12CB11AA116D1A08090C12EB116911EB112C1AE7088E1A2909",
    "2C1AAA09EB11AA11CB112C1228098E1AE7084D12CB118A112C128A110C12EB11CB114D12EB116D1A2C12D7345935AE1A8A116911EB112809CB1189118A11AA114909EB112809AB1169116911691169118A112809AA11E708CB11C708CB11080969118A11E708EB1186000C126500EB11860008096911080969112809490929094909290928092809280928092809280928090809280908092809280928092809280928094809280928092809280928092809080928094500EB114500EB11E708EB118A11EB112C1A",
    "8A118E1A6911AE1ACB116D1A4C1A4D1AEF22CF22B22B912B922BCF22CB118A11450008094500A600C700450028094500A60049094500A600C7004500280945008600E700450008094500450008094500C700A6004500290945008600E7004500080965004500280945008600E7004500E708C700C70828094D220C222D2A2D2A6E2ACB218E2A4D2AEC21CF324D2A2D2AF0324D2A6E32113BF03A1243D0320F12B4029A541CA6BFF7FFFFFFFFFFFFFFFFFFFFFFFFBFF71CA69A54B402AD094909E8000C127223962C",
    "752C7123922B1023312330235945183DD73C7123EC112C122C12AA118E1A69094D1AAA110C120C12CB11EB110C12CB118A094D1A49096D1A8A110C1269094D1A69090C12CB11EB1169096D1AC7006D1A6909CB112C1208094D1A49092C1A69092C128A11EB11EB118A11EB110C12EB11CB116E1ACB116D1A962CD734AA116911AA110809EB1128098A1189118911AA112809EB110809AA116911691169116911AA11E708EB11C700CB11490949098A1108096911490949096911490949116909A600CB11C700CB11",
    "A600AA11A6008A11C7086909070949090809490928092909280928094809280948092809280908092809080928090809490908096911C7006911A7008A1145008A11A6008A11C700691108098A1149090C1AEB110B1AEB11EB11EB11EB19EB192C1A0C1A8D1A2C1AAE22CF22302BB22B712B712B4C1ACB112809450008094500E700C7004500C7008600450008094500C7008600450028094500A600A7004500080945006500E7084500E70086004500280945008600E7004500E708860045006909450049096909",
    "AB19EB19CB190C22CB218E2A0C220C228E2AEC212D2A8E2AEC296E2A8E322D2AAF326E2A6E32F142B03A912A710A7302982339443944394439449823530210026B098A11A7008E1AB223762C342C71239223302330235123CF1A59455845342C2C120C124D1AAA112C1AEC11EB11AA110C12EB118A112C128A096D1AE7086D1AAA1169112D126911CB112C1A28096D1A6909EB11EB11AA11AA11EB116909EB118A110C1249096D1AA600AE1AC7004D1A8A1169098E1AC7084D1AEB1169098E1A69098E1A8E1A352C",
    "18358A118A110809EB11E7088A116911891149096911AA11E708CB1128098A1149098A1169094909CB11E708CB11280969094909AA11E708CA11080969098A11C700EB11A600CB114809C700AA11080949096911C700AA118600EB116500CB118600AA11A6008A11E70849090709280928090809490908096911E7086911C7006911C7008A11A600AA118600AA11650008092809E7086911C70089118600CB11A600EB1169114D1A8911EB11EB118A114D1A69118E1A69116D1ACB112C1A6D1A2C1AEF22CE22512B",
    "912B502B512B2C1A0C1269098600E7086500650008094500E7086500450008094500C7008600450028094500A600C700450008094500450008094500C700A6004500080945006500CB1145008A1128090C1A8A19CB19AA19EB19AA19EB212D220C222D2A0D222D2A0D2A2D2A6E2AEC296E2A6E320D2AB02A2D2A6F32D03AB03AD032AC1929092809C8002909080108010C12CF1A34245524342C302371233023EF223023CF1A502358455845CF1ACB114C1ACB114D1A8A094D1AAA11CB114D1A49092C1AAA090C12",
    "AA11CB110C12AA11EB110C12CA114D124909CB112C1208092C1AAA118A110C12CB1169094D1AC7008E1A08092C1A6909EB116909EB116909CB11EB1149092C1A69092C12AA110C126D1A4D1A142CD7346D1A08094D1AE7088A11490969090809AA1169090809CB11E7088A112809AA1108098A1189110809AA11280969090809CB11A600EB11080969098A11080969116911080989114909E7086911280928096911C700CA11C700AA11E70849094909E708AA11A600CB11A600AA11C7006911E70828092809E708",
    "6911C700AA118600CA116500CB114500CB114500CB11A600A6008A116500CB114500EB1145000B124500EB1165008A1169112C1A8A11CB11AA11EB11CA112C1A89114D1A28098E1A69118E1AAA114D1A2C1A2C1ACF22AE22502B912B502B712BCF222C1A0C12E70828096500450008094500E7086500450008094500C700A6004500280945006500E7004500E700860045002909E7088600CA114500AA11C70049110C1AAA198A190C1A8A19AA192C226A19EB214D2A6A196E2A2D2AAB21AF2AEC292D2AAF32CB21",
    "AF324E326E32F03A4E32934B324B11434D224911EF22F32B962C342CF32B302371231023EF223023CF1A5123EF227945173D0C124C1A8A096D1AEB11CB110C128A112C1A8A11EB11AA112C1A49094D12AA118A112C1269092C128A090C128A09CB1169092C1228092C1A49090C1249092C124909EB118A098A11EC116911EC11AA118A092C1A08096D1A0809EB11EB1169096D1A4909CE1A4D12F323D72CAF1A0C12C700AA114809AA11A600EB1128092809CA11E708690948098A11C708EB11280949098A114909",
    "49096911AA11C700EB11E7088A0948098A11E708CB11E70889118911E7088A11CA118600EB11C70869096911280969114909E708AA11A600EB118600CA11C7084909280908096911E7086911080908094909C7008A116500CB1145000B1245000B124500CA11C7084500EB1145000B124500EB114500AA11A6004909080907094909A6000C1249110C122809EB118A11EB11CB11CA11CB11CB11AA112C1A69116D1A69116D1AAA114C1A6D1A0C1A4C1AAE228D22712B102B912B302BAE228E1A89118A11E7084500",
    "28094500A600A6004500080945006500E7084500E70086004500280928098600AA114500AA11E7084D228A198A194D220811CB190C2208110C22AB1949112C22AB19CB212D22EC210C220C2A2D2A0C2A4D2A2D2A4D2A4D2A6E324E32CF3AB03A564CD844D73C14347123302330233023CF1A3023AE1A3023EF225123794595342C128A090C120C122C128A11EB112C128A092C1A8A092C12AA11AA114D1A28092C1ACB118A118A116D1AC7004D1A45008E1A4909CB110C128A11AA112C1208096D1A0809EB118A09",
    "8A11AA11CB1149094D1AE7082C126909AA118A09EB11CB11EB11EB114D1A4D1AB223752C5123CA11280928094D1A6500AA11490908098911690908096911AA1186000B12E7086909691169110709AA116911E708CB11080969094809AA11A6000B12C7086909691169090809CA11C700EB11491169090709CB1165000C128600AA1108094909491149090709AA1165000C1245000B126500AA11C7084909280928094911280908096909C7008A118600AA11A6008A1149096500CB114500CB1186008A11C7002809",
    "2809C7008A118600CA116500AA118600AA116909AA116909AA11EB11CB11AA11CA1169110C1A89112C1A8A11AA11EB196911EF2269110B1AEB11EB194D1A0B1A0C1A8D1A2C1A1023CE22302B502BEF22502B6D1A0B1AEB11E70849096500450008094500C700A6002809280928096500CB114500691149094911AA190C1A2811EB19AA198A19CB19AA19CB1969110C228A1969194D2249112D2A0C2AAB21AE2A6A198E2AEC292D2A6E2AAF32353CB73C963C553CF43B564C1544353C712B30238E1A1023CF221023",
    "B22B7845B63CEB110C128A114D1AEB1149096D1A69092C1A69094D1A6909EB11CB11CB11CB118A112C1A28094C1A6909CB11EB11AA118A090B126909EB118A11CB11AA118A11EB1149094C12E7084D1A08092C1A28090B126909CB11AA11AA1149094D1A49092C12CA112C12CF1A3023752CEF1AEF228600690969114909C708CB118600CB1108090809CB11A600AA11280949090809CB118600CA11490908098A11691108096911AA11A600CB11080969094809AA1186000C12A6008A114911CB11C700EB11C708",
    "6909691128094809AA118600EB118600AA11080949096911080928096909A600EB118600CA11C70049094909C700AA118600AA11A6006911C70849092809490908092809E708491108094809280907096909C7006911A6006911C7006911080928094909E708AA11080969118A11EB19CA11AA116909CB1189112C1A08092C1A280969118D1A07096D1A07092C1AAA11CB112C1AAA112C1AEB11EB196D1A0B1A4D1AAE226D1A512BCE22502B302BCE2210232C1A2C1ACB11AA11490969114500CB1145000809EB11",
    "AA19AA19CB196911AA19EB1908110C22AA1929114D224911AB19EB1929110C226919AA19EB21AB216D2AAB216E2A10337634B7345534F433D333B333B333D33BB33B1444F443354CB33B512B5023B22B58455434CF228A11EB114D128A11EB110C12CB118A092C12AA118A116D1A49092C1ACB11AA118A092C1A08092C12AA1149096D1A49096D1A6911CB112C1228094D1A6909AA110B1208096D1AC7002C1A4909AA11EB116911CB11EB11E7084D1A08098D1A08098E1ACB11CF1A5123F32B92234D1AAA118600",
    "CB11A6008A114809080969110809E708AA11C70069118A1186000C1286006909691149090709CB11C70069118A11E7086911691108094909CA11A600CB11280949092809CA118600CB11AA11C7008A116911E708AA1108092809AA11C7008A114909E708AA11E70849096911A600EB118600AA11E708280969112809280969098600EB1145000C124500AA11C7006500EB116500AA11C70828094809E7086911C7006911C7084909E7084909280928092809E7082809E7082809280969092809AA112C1A4909CA11",
    "28090B12CA11AA11CB118600CB11AA118A112C1AA6008D1A07096D1A69098A112C1269114D1A6911EB19EB110B1AEB112C1A8A118E1AEB196D1AAE220F2B302B502B2F2BB233EF22712BAE228D2230238A190C22E708EB19AB1928110C1A6919AA198A19CB198A198A19EB1908112C224911AA19CB194911AE223133D73C96341434D333922BD333312BB33372335133B23B7233923BB33BF443154C7654F74C753C8E1AAA11EB110C12EB118A098D1AA6008E1A6909EB11CA110C126911EB112C1AE7088E1A4909",
    "CB110C128A11CB11CB11AA118A09AA11CB11AA118A11EB1169090C1228092C1A4909EB118A11CA11AA11CB1149092C1A6909EB1128096D1AE7086D1A69098E1A2C125123B2237123EF1A8A11280908092809E7084C1A65008A1108090809AA11C70069110809080969114909C700EB114500CB11E708280969116911A600CB11E7082809AA1108090809AA1108090809CB11C70089116911A600690969116909C708EB118600CA112809280949116909A6000C126500CB11E708490949116909C708CB1145000C12",
    "65008A110809E7088A11E70849092809E708691108098600CA118600AA11C700E70869116500EB114500EB1165006909E708C7008A118600EB116500AA11A60028094911C700AA116500AA1108090C1A8A110809EB1928098D1A2809E7082C1AE7088D1A48096911CA110809AE22C7082C1A2809AA112C1A08094C1A48092C1ACA11CA11CB11EB194C1AEB11AE228A11EF224811EF22EB194C1ACF222C1A302BCB194911CB198A19CB192811EB19AA1929114D220811CB198A1929112C220811EB196A19CF2A5434",
    "963475347123F3333023B233512B512B7233312B7133102B92331033923B7233F33B3544D74C764C3554723BAE222C1A49094C1A6909EB112C1228094C1A8A110B128A110B1269092C1A49092C128A118A112C126911EB110B126911CB1128094D1289118A092C1AC7008E1A86006D1AC7002C1A2809EB118A118A11EB11CB1128092C1A49094C1A69094D1AEB118E1AEF1A71235123CF1A2C12491108096911C700891169094500EB118600AA11E70808098A11A600CA11A60069094809280908098A1165000C1A",
    "65006909491128090809CB118600AA114809080969118911A600CB11080908094811EB114500EB11E708490949118A1186000C1286008A114811280948098A11A600EB11A60049096911C700AA11E7082809691108094809690986000B1245000B1286004909CB11860069090809490949112809E7086909A6008911E708280949098600CB114500EB118600490928098600CB1145000B126500690908098600EB11E7084C1A8600CB118E1A28090B1AE70828094D1A08096D1AA600EB11EB1128092C1AA6006D1A",
    "69116911EB1128096D1A4909CA11CB116D22CA112C1ACA118D1AEB19CB112C1A2C1A0C1A4C1A2C1A0C1A0811CB19CB19E8102C2269118A19AA19AA198A19AA19AA1949110C22AA19F33375349634F32B922B712B712B3023302B1023EF22EF227133CF2A7133102B7133302BB23B7133554496445544F3431033723B934BD44B10336D228A112C1249094D1A4909CB112C1AC7008E1A2809EB112C128A11CA110C124909CB11EB1149092C1A49090C1AAA11CA116909EB116909CB118A098A11EB1148110C126911",
    "8A112C1A08090B1249092C1249092C12CA112C126D1ACF1A51230F1B10230C124909280908096911C700AA1108098600AA118600CB118600AA11C70028096911A600EB1186008A11E70828094911490986000B124500CB11E708280948118A1165000B128600690949114909C708EB11650008098A112809E708CB11C70049098A11C70069116911E7088A114909C700EB1186008A112809280928098A1165000C1A4500CA11E70808098911E70808096911E7086911AA116500CB11C700080989116500EB114500",
    "AA11C70828096911E7086911C70828092809490928092809C7086911C70069112809C7088A116500AA11C70048098A11E708EB1149098A114C1AC708EB1148090B1AEB11A6000B1228092C1A8911E7080C1A69110B1A490989112C1A0B1A89112C1AEB196D1A48092C1AEB116D2269112C1A0B1AEB11CE220811EB198A11CB192911EB198A1949112C22C7080C226919AA19302B342CD734142C14343023B22B0F23712BEF221023302B8E1A512B8E220F2BAE22302B102B30337133B23B143C7544143C923B3133",
    "8F2A31338F32513B5243D44B9343CF2A0B1269092C1A69110B12AA11EB116909EB11AA11CB118A110C126911AA112C1AC700AE1AC7001023C7006D1A28092C1A6909EB118A11CA1169090B1208092C1A8911CB1128094D1A49090C128A110C122C124D1AEF1AEF1A51236D1AEF1A08096911070928094909E7088A110809E7086909C70069118600AA11A600AA11C70049094909A600CA1186008A11C700280969110809E708AA114500EB118600490949112809A600EB114500AA1108092809E708891108090809",
    "CA116500CA112809280949118A1165000B1A8600690949114809E708CB116500EB11E7080809AA11C70049094909E7084911690986000B124500CB11C7088600AA11C708490949112809E7086911A600AA110809E708AA1145000C1A65008A11E708A600CB116500CB11860028094911C700AA1186006909070928094911E7084909691108094909E708EB110B1A69092809AA11EB11AA1169096911EB11AA1149118A11EB19AA11CA1148098D1AEB19EB1969096D22EB114C1A08096D1AEB198A114C1A4911CE22",
    "2811AA19CB19E7082C2249118A198A19AA196911AE22D22BB6349534D32BD22B3023712B3023EF22512B8E1A712B8E1AEF22CF22EF2AAE22EF22EF2ACE22302B102B9133133C343CD33BD33BEF32102B6E2AAF2ACF2AF032CF32F03A313B7343154CF03AEF2A6909EB11EB11EB1169116D1A0809EB11EB1149090B1269090B1228092C1A08098D1A28090C1228090B1228090B1228090B126909CB118A110B12C7084D1A49092C1249090B12CB110C126D1A6D1A3023AE1A10232C122C12C7008911E70869092809",
    "08096911280908094809E7084909C7086909A600AA11A600AA11A60069110809C708AA11A600AA11C70008096911C70049094909A600EB11A60028096911E70849096911A600AA11AA114500CB11070928092809AA1165000B1A8600280969110809E708CA11A600691169118600CB11C700480969114909A600EB114500EB11C70808098A11E708480949090809C708CB11A6006911480986000B124500CB11C70828098911C70069090709280949114909C7088911A6008A110809C700CA114500EB1186000809",
    "891145000C1A45006909EB11450089110709A600CB1186006D1A080908094C1A49110C12C70889116D22E708CB1108098D1A2C1A48110B126D2269118D22C700CE22AA1128098D2269112C1A6911CA11CB1969110B1AE7082C2269196A192C2228115534F7349534D32B712B922B0F23B22BAE22922B8E1A512BAE22AE220F23502B6D1A8E22EF224D22EF2ACF229133712B753CB233D2335033D3338E226E2AAF2A102B0C2A30336E2AF032F0323133F03AB343155452438D2228092C1A49092C1A2809AA112C12",
    "C7006D1A6911EB11CB116911CB1169110B1228090C126911EB118911EB118A118A112C1A6500AE1A69090B12A600CB116909EB11EB112C12AE1A8E1A30236D1AEF222C12CB11C7008911E708690928092809490928092809280908094809E7086909C70089118600AA118600AA11A60049092809C708AA11C70069110809C700CA11A60069112809C700AA11C700C700CB1186008A1128098909A6002C1A650069112809E70889114809A600CA11E708A6000B1A6500691169112809E708CB1145002C1A86000809",
    "AA11E70808098A11E7084811691145004C1A45008911EB196500690949116909E70889118600CB11E708C708CB1145002C1A450049096911A600EB1165006909280948094909E708080928096909E708691186008A110809E708EB11E708E708AA114500EB11A60007098A1107098D1A860028098D1A08090B12A6006D226D1A0709EB114C1AAA118D1A4500302BAA1128092C12CA11AE22480907090F23AE22CB19AA196911AA198A19AA196D2234347534B634D22BD22B712B5123502BEF223023EF22EF22CE22",
    "8D228D1AAE222C1A2C1A6C1A2B1A6C222C1ACE228D1A302B0F2BB233912BB233502B8E22EB19F02AAB21CF2A8E2A6E2AF02A8E2AF032F032AF321133523B7243354C113BCF2ACB118911CB118A110B1208094C1A6911EB11CB114911EB1169112C1228092C124811CB11CA1128096D1A65008D1A08090B12C7004C1A49092C1AAA110B124C124C1AEF1A8E1A0F234D1ACE1A28098911C7086911E708690908092809480928092809280908094809E7086909C70869098600AA116500EB116500AA11A60049092809",
    "E7086911E708080949096500EB1165008911080908096911490965000B1A45006909AA11E708E7088A11E7000809AA116500CB11C70828094911690965002C1A650069096911E70848096911C7006911490986002C1A650089112811280928096911A6008A11AA116500CB11C700C700EB116500AA11E708490969112809E708691108092809891145000B1A86006911490965002C1A450089110809C700EB1165008911070908098911860028094911E70828092809490928092809080948114909CA11AA11E708",
    "EB192C1AEB1969090B1A0B1A6D1A8600AD22CB114C1A6909CB116D22EB1128092C1A4C1A89110B1A49110C22C708EF2A712BB634B634F32BD22B712B712B302330230F23EF22CF22EF228E22EF222C128D1A8E1A700AD302F702F702F702F702F702F702D402D212EF22702BEE22502BEB19CE22CB19CB19AE22AA198E224D228E2A6E2AEF322D2251334D2A3033AF32513B313BB44BF54B10338E22CB116911EB11AA116909EB114909EB1149090B1A8911CB1128092C1AC7006D1AE7082C1A28094C1A65004D1A",
    "48092C1A69092C1AEB112C1A6D1A6D1AEF1A6D1ACF1A2C1A8D1AC7088911E70869110709490908092809280949092809280908094809E7086909C70089118600AA114500EB114500EB114500CA11860069090709280948112809C7088A1145000B1A4500AA11E7082809491128098600CB19490965002C1A45006911280928090709AA114500EB11C600E708AA11C708280969110809E708EB114500EB11C708280969110809C708AA11E708E708CB1145000B12A600A600AA11E708490949114809A600AA11E708",
    "0809AA1145000C1A8600280989118600CA11A600490969110809280928092809281149098600AA11E7082809891145000B1A080945000B1A86000809AA1145000B1A8600E708CA116500CB11C70028098D2286006D1AEB19EB11CA11AA110B1A8D228600CE228911EB11EB116911AE228A1148116D1AAE224D22922B9534B6345434B22BD22B0F23D22BAE1A912B8E1A512B8E1A302B8D1AEE228D1A0B12B01AB402F702F933DA64DC951C9EBDC6BDC61C9EDC95DA64B82BF702D5026F1AAD222C1289112C1A6D1A",
    "6A11AE228A194D1A2C22EB19CF2ACB2130332D22CF32F02ACF32F03231331033723BD44BF44B1033AE2269118A110B1AE7088D1A65008D1AAA11AA116911AA116909CB1108098D1A86002C1A28096D1A49092C1AAA112C1A0C126D1A8D1A8D1ACE1A6D1A8E1A0B12690907096909080949092809480928092809480969090809480907094909E7086911C70089118600AA116500EB1145000B124500EB1145008A11A60028092809280948112809C70089116500CB11860028094809A600AA116500280928094909",
    "6500EB19860028096911A6008A11E70808096911690965002C1A450069094809E70848094809E7086911690965002C1A650049096911E708480928090809C708CA11C60028098A1145000B1A650028098911C60069110709480948114909A600AA11E708E708AA1145000B1AA600E708CB114500EB11C600E708CA1165008A110709C7006909280928096911E708280969112809080948092809481149092809EB196909AA1169118911EB196D1A86008D22CB114C1A6911AA118D22EB11E7082C1A6D1A49114C1A",
    "F634132C1334712B712B502B302310231023EF22EF22EF22AE220F2B2C1A2F2BEB118E1AB302F70259441C9E7DB67DB67DB67DB67DB67DB67DB67DB67DB67DB61C9E5944F702B30269094C1A8911CB196D1A4911CF2269118E22CB192C224D1A4D220C1A8E2A8E22CF2A8E2A51334D2A3133F03211339243F44BD44B31338D2228092C1AE708EB11CB11891169110B12C7086D1A0809CB1169114C1A69110B1A89110B1ACB112C1A2C126D1A8D1A8D1A8D1A6D1A6D1AAA1128092809280928092809481108094911",
    "08096911690907096909E7086911C7088911A6008A118600AA116500CB114500EB116500CB1165008911C60008092809C7008911A6006911C70808094811E708481128098600CB114500E708891186008A11C6000809691128098600EB114500CA11C708E7088911E708070969110809A6000B124500CB11E708E7088911E70808096911480986000B1A65008911EB11450069094811E708691108092809481169096500EB11A6000709AA1145000B12A6000709CA1186008A11070928094911E708080969114909",
    "C7088911E70848116909860008098A116500EB11E708C600EB116500AA11280965006D1AE70889110B1AA6006C1AC7086911EB1148114909AE2286008D1AAA11EB190B1A28096D1A2C1A08090B122F2BD22B3023912B30233023502BCE22502B6D1A712B2C1A712B4C1AEF2A6C226C1A4F12F602370B5B7D1CA61CA61CA61CA61CA61CA61CA61CA61CA61CA61CA61CA61CA61CA65B7D370BD602AC094C1A28116E226A116D22AA116D22CB192C1A2C1A4D22AA19CF228A118E220C1AEF2A2D22EF32CF328F2A5133",
    "AF327233523BF54BD443723B6D222C1A08094C1AE7080B12AA11691189110B12CA11CA118A11CA11EB11EB112C122C1A6D1A6D1A8D1A6D1A6D1A4C1ACA11481108094811080948110809691107096911070969118911E7086911C7088911C6008911A6008A11A600AA118600AA118600AA11A6008911C60048110709E70869098600CA116500EB1145008A11C6002809481128090709690986000809690945000B1A4500AA11C708E7088911C708E70869110709E708AA1145000B1A860028098911C70028094911",
    "0809E708CA114500EB11A600E708CB118600690948110809E7086911490965000B1A65004911691165000B1A860028098911C70048092809490907096911A6008911480965000B1A86004809891145000B1A8600E708EB1165004809EB19650069094811E708AA11C6004809891107096909AA118911AA1169118911EB192809CA114811EB112809EB1948092C1A2809CA11CA112C1A891169116D1AEB190B1AEF22502B0F230F23EF220F23CE22EF22AE22CE22CE228E22AE226D226C1A9012F702370B5B7DDC95",
    "DC95DC95DC95DC95DC95DC95DC95DC95DC95DC95DC95DC95DC95DC95DC955B7D370BF7020E0A0B1289114D1ACB19EB196D224911CE226A118D226911CE228A114D222C1A0C1A0C1A4D226D224D22102B4D2A51338F2A5133F032B33BB4433544D443102BAE2249110000000000006911CA1148090C1289114C1ACA116D1A0C128E1A2C1A8D1A2C1A6D1A0B1A6909280928092811280948112809481128094811280948118A11A600AA11C608AA11A600AA11A6008A11A6008A11A600AA11C7006911E70848110809",
    "E7086909A600AA1145000B1A45000B1A45008A11A60008094811C7008911A600690948110809E7086911A6008911E708A600CA114500EB11650048096911E70807094811E7082809691145002C1A650028098911A600691108092809080989116500CA11E7084500CA11E708A600EB19650069096911080907094911280908098A116500CB11E70886002C1A4500891148118600EB11860028098911A600490948114809280928092809E7086911280907096911C600891148098600CA11891108092C1AE7080B1A",
    "8A11E7086C1A28096911CA11691169092C1A86008D2208094811EB11691148110B1A891148118D228E1A502BCE220F23EF22CE22EF228E22EF228D22CE22AE226C1AAD1A6F12F702F70259447B857B857B857B857B85FC9DBDC65EE75EE75EE75EE7BDC6FC9D7B857B857B857B857B855944F702F702CD092B1A48116D22AA118E222811EF228A114C22EB194D22CB190C1A6D1ACB194D1A2C22EB194D220C1A4D224D228E22AE2AEF2AAF2AEA1900000000C919F54B0000FFFFFFFFFFFF00002C1A08096D1A6909",
    "6D1AAA11AE1AEB11AE1AEB11AE1AEB118D1AE7088911C7086911E7086911E7086911E7086911E7086911C708EB11A600AA11A600AA11A600AA11C7008911C7006911E7084811280907094909C7088911A600AA116500CA114500EB196500AA11A60028090809A600AA114500EB1945002809AA1186008A11A600280928094809070969116500AA11A6000709691145000B1A450049094811E7082809080908094811690945000B1A8600E708AA116500CA11C7082809280928094909E7088911C600691169094500",
    "2C1A8600E708CA116500AA11070907098911C708480948112809C70889112809C608AA11A600691169114500EB19C700A6006D1A070986002C1A65002809CA1165004C1A280969110B1A281148096C22C600CB11AA1107092C1AE7088911EB1169110809AD2286000B1A89112809EB1149114809EB112C1ACE220F2BAE22EF22AE22AE22AE22AE228D22CE228E222D1A0F23AB11F602F702F702FA643B753B753B751C9EBFEFFFFFFFFFBFEF5EDF5EDF7FE7FFFFFFFFBFEF1C9E3B753B753B75FA64F702F702D602",
    "6D1AE7088D22AA112C1ACB116D22CB19EB194D1A2C1ACB194C220000000000006D22AA19CE226911EF224911EF224911EF2206090C63FFFFFFFFEF7BC9190000FFFFFFFFFFFF0000F33B2C1ACE224809AE1A8911CE22AA11CE22AA11AE1A8A114C1A6500CA116500CA118600CA118600CA118600CA118600CA1186004C1AC7006911E7086911E7084811280908094909E7086911C7088A11A600AA118600AA116500CA118600AA11A6008911C70028112809C6088A116500EB114500EB19650065000B1A4500EB11",
    "860028092811C600AA1165006911E70828092811280986008A11C6000809690945000B1A450028096911A6008911C7084909481128098600AA110709A6006C1AE70886002C1A450069094811A6008911C708490948112809E708891149096500EB11C600E708CA114500CA11070986000B1A65002809CA1165008A110809E708AA11E708E7086911070928096911280948092B1A0809CA116911EB110709EB196909AA1189118911CA1949098A116911EB1108092C1A28090B1A2809AA11AA112809AA11AA11CA19",
    "302B8D22EF22AE22AE22AE22CE224C1A302BEB19502B4C1A0A1AB402F702F702F702FA64FA641B6D1ECFFFFF9FEF3DAE1B6DFA64FA64FA64FA641B6D3DAE9FEFFFFF1ECF1B6DFA64FA64F702F702F702B30248112B12EB19CB192C1A4C22CB112C1A0C1A4C2269110000FFFFFFFFFFFF00008A11AE226911CE2269118E22AA194D2247090C63FFFFFFFFEF7B47110000FFFFFFFFFFFF0000143CB43B7644D33BF3338E220F238911EE226909CE222809EB1945000B1A45000B1A45000B1A45000B1A45000B1A4500",
    "0B1A4500CA114809E7086909C7088911A600AA118600AA118600CA116500CA116500CA118600CA11A6008911C7006911080907096909C608AA118600CA116500CA11A6006911E7084500EB19860069110709A600AA1145000B1A45006911E708C700CA1165008911C708280948110809C6086911E7080709891145000B1A8600C700CA114500CA11C6080709891186002809280928090809481169096500CA19E70886000B1A4500891148096500EB1186000809AA11860049094811070928092809490907094911",
    "0709481169098600AA116909C600691169096500CA11EB196500AD22C700CA19CA11691149112B1A86006C224909E7082C1A48092809EB19891107096C1AC7000B1A69116911AA1189114909AA110B1A302B6D1A0F2B2C1A0F2B2C1AEF2A6D1AAE22EF224D1A0B12901AF702F702F702F7029A54DA645EE7FFFFBDC6DA649A549A549A549A549A549A549A549A54DA649DB6FFFF5EE7DA647A54F702F702F702F7020E0A2B1A89112C1ACB118E222809AE228A118D2228110000FFFFFFFFFFFF0000CB112C22EB11",
    "2C1A0C1A0C1AEC194D22E6080C63FFFFFFFFEF7B67110000FFFFFFFFFFFF0000302BAF2AB2333133143CF43B7644F43BF33BAE22CE228600EB116500EB116500EB116500EB116500EB116500EB116500CA118600C600CA116500EB114500EB1945000B1A4500EB194500EB196500CA19A6008911C7086911080907096909C7088A11A600AA118600AA11A6008911E70848112809E7088911450069112809C70889116500CA116500AA11C600E708891145000B1A45008911E708A600CA114500AA11C60808096911",
    "A600280928094809E70869118600891128096500EB19860045002C1A450007098A116500AA11C70808096911A600280948112809C60069114809A600AA11C7000709AA116500AA11080965002C1A8600C708EB194500691189114500E708EB196500CA1169116911AA11CA19C7008D228600CA19CA11691128096D228600EB11CA110709EB1169114809CA11CA11A600AD22C70869110B1A691107090B1A0B1AAE22AE228D22AE226D1ACE226D1A0F2B6D1A6D222B228D22B402F702F702F702F702D82BDEC6FFFF",
    "BC8D59445944594459447A545EDFBC8D59445944594459445944BC8DFFFFDEC6D82BF702F702F702F70294024B1AE708AE226911CE22E708AE2269116D2269110000FFFFFFFFFFFF0000AB114C22CB114D228A118D226A116D2247090C63FFFFFFFFEF7B26110000FFFFFFFFFFFF0000AE226D22CF22AE22102BEF2A712B5133F33B143C3544D43BB33BAE226D2207098911C7088911C7086911E70869090709480928094500EB194500EB194500EB196500CA1186008911C7086911070908094809C7088911A600",
    "CA118600EB116500CA118600AA11C7006911080928096909E7086911A60089110809E7086911A6008911A6006911070928096909A6008A118600AA11E708E70889114500EB1965004809480945000B1A4500691107098600EB19450069094811E7084911E7086909E70828114809C7088911A6006911480945000B1A8600C600EB1145008A1128096500EB198600E708EA19650069096911C7006911070928094911E7084809481107096909070928096911CA192809CA114811EB1107092B222809CA114811EB19",
    "E7082B1A2809EA194811AA11AA114811AA11AA11891128096C22A600EB19AA1128096911EB19AA114C1AEF2A0B1A502BCB11702B2C1AAE222C1A0F2B4C126D1AF702F702F702F702F702BA5CFFFF7DB6F933F933F933F933F933FA6CFFFFFECEF933F933F933F933F933F9333DAEFFFFBA5CF702F702F702F702F7024D1228118D22CB118D2249116D22CB110B1A0B1A0000FFFFFFFFFFFF00006A116D228A116D226809E508A400A308C5080C63FFFFFFFFEF7BE5080000FFFFFFFFFFFF00006D1ACE228E22CE22",
    "4D1AA811C408C40847112C1A4D22AE2AF02A923BD43BF443F43B7133CE222C224809691107096911C708891145008911C7086911E7084909280908094811C70889118600CA116500EB1945000B1A4500EB196500AA11C6006911080908096911E7088911A6008911C6086911E70869116911E708691107094811280928114809E7082809E7084811280928114909C7084911C60069112809C70889116500AA11E708C708CA114500EA19A600A600CA114500CA11C7084500AA11E7088600EB19450028096911A600",
    "6911E70828092809E708280948112809C700691148098600AA110709C708AA11A600481189116500CA19080945000B1AC60045000B1AAA11A6006C22280907094C1A2809E7088D22A600CA19AA11691148094C1A86004C2269092811CA19AA110709EB19AA11C7084C222809EA194811CA116911EB19AA11CB11EF2A4C1AAE220F2B2C1A6D22CE2A6D1ACE222C1AD212F702F702F702F702F7027EE77EE7F933B823B823B823B823B823BA5CFFFFFECEB823B823B823B823B823B823F9337EE77EE7F702F702F702",
    "F702F702701289112C1A2C1A2C1AEB114C220B1ACB194C1A0000FFFFFFFFFFFF0000EB11EB192C1A84000421EF7B95AD139D694A2C63FFFFFFFFEF7BE5080000FFFFFFFFFFFF00002C1AEE224D1A871182102C6354A595ADEF7B2421A408EB190C1A0C1A2D1A2D228E22F02A9233D43B143CF43B5233EF2A2C1ACA19070908096911E7088911C600AA118600CA1965000B1A45000B1A6500CA1186008911C70848114809C70889118600CA116500CA198600AA11C600891108094811490907090809280948112809",
    "2809280928090709280928112809481107094811E7084809281128094811E7082809E70828094811280907090809080948114809A60049110709281169118600070989116500CA1907098600EB1965004811691145000B1A860086002B1A4500490969116500CA11E708C600EA1965000809CA19650069098911A60069112809280969092B1AE708CA1189114911AA1189116911EA11691148092C1AC608CA11CA1169110709AD226500CA110B1A280928094C1A070908096C22C700CA198911CA1128094C1ACA11",
    "2F2B2C1A2F2B8D222C1AEF2A4C1ACE22AE22EE2A2B1AB302F702F702F702F702F933FFFF3B751703581358135813581358137A54FFFFDEC658135813581358135813581317033B75FFFFF933F702F702F702F702B302C9112C1A4D1A2C1ACB116D22EB11EB194C1A0000FFFFFFFFFFFF00006D1A491183085084DEF7FFFFFFFFFFFFFFFF5CE7FFFFFFFFEF7BC5080000FFFFFFFFFFFF00002C1AAE22A4084D6BBEF7FFFFFFFFFFFFFFFFFFFF718C62002C1A8A116D1A8A116D228A116D22CB198D220C1A102B312B",
    "D33BD43B143CD3337133CE226D2249118911C7088911E7088911E7084909280907098911A600CA1945000B1A45000B1A4500CA19A6006911070907096911E708AA11A6008911E708A919491107092809280908094809481128096911C6008911C60869112811070989118600AA11A60069094811C600AA11650069110709E708891165006911070907096911A60069096911A60049094811E7082809E708490928110709070948112809A60069114809C7086911E708281169118600891148096500CA1908098600",
    "CA11C708E708CA11691145008D2208096911CA118911E7082B1A0809A911CA112809CA19891169118911EA11E7082C222809EA194811CA114809CA192809CA11CA11A6004C1A8911AA110B122B1AEB112F2B6D1A8E222C1A0F2BEB19502B6D22EF2A2C1A8D22F702F702F702F702F7027B85FFFF5813F702F70217031703170317035944FFFFDDC617031703170317031703F702F7025813FFFF7B85F702F702F702F702F70268116D220C1A6D22AA11AE2269118D22CB110000FFFFFFFFFFFF0000AA11E4083084",
    "FFFFFFFFBEF734A5918CDBDEFFFFFFFFFFFFEF7BC5080000FFFFFFFFFFFF00004D1A0509AE73FFFFFFFF59CE2842494ABAD6FFFFFFFFCF7B261108098D2208098D2228098D2269116D22AB114D22EB192C1A2C1ACB19AE226E2251337233D33BF433B33B512BCE2A0B1ACA19C708AA118600EA196500EB198600AA11C60848092809C708AA116500EB196500EA198600A911E70848114809A6006911280948114809481107096911C7088911E70848096911A600CA196500CA11A6082809891165000B1A45008911",
    "070986000B1A4500AA11C708A600EB194500AA11C7084500AA11E70886000B1A4500480969116500EB118600C708EA194500691148118600CA11C608E708EA1945002809A911650069114811A600AA11E70828090B1AE708891189112B22A6006C22E7088911CA118911C7088D22C60869114C1A070928094C1A2809C708AD22C7008911AA11CA19A6004C2248096911AA114809EB19AA11EB194C1AEB19EB118E224D1A0F2B4D1A0F2BEE22AD226D1ACE22CE224C1AF702F702F702F702F702BDC67EE7F702F702",
    "F702F702F702F702F7023944FFFFBDC6F702F702F702F702F702F702F702F7021ED7BDC6F702F702F702F702F7020711CE22CB118D228A11CE222809CE2249110000FFFFFFFFFFFF000027094529DEF7DEF7BEF7A631A300A4002000D294FFFFFFFFEF7BE5080000FFFFFFFFFFFF00000B1A4529DEF7DEF758C64108A911A500C3109DEFDEF79DEF620807096D2249112C1A6911EB19AA11AA11EB198A114C1A8A118E2249112C1AEB190C1A4C1AEB196D222C1AEF22F02A9233B333D333D333302BEF222C22AA11",
    "8911A600CA1945000B1A45000B1A6500AA11E70828096911C608CA118600AA11E70807098911E70848112809281148094811070989118600CA11A60089112809C600EA1945000B1A65002809691165002B1A4500891107098600EB194500CA19E7088600EB1965008600EB196500691148094500EB19A600E708AA114500AA1908096500EA19A600E708AA116500891148094500EA1907098600CA11C700E708AA11481169110B1AC70828114C220709CA196911CA19E7082B1A07090B220809CA196911A9114809",
    "EA1928098911EA19E7080B1A6911691169114C228600EA19CA11AA11A9112C1A0B1A89116C228911CE2AAE22EF2AEF2ACE22EB190F2B8D22502BEB19CD22F702F702F702F702F702BDC6BDC6F702F702F702F702F702F702F7027B85FFFFFFFF9823F702F702F702F702F7023B75BDC6BFF7BFF7BDC69A54F702F702F7020711CE22CA118D22AA11AE226911CE2269110000FFFFFFFFFFFF0000E508918CFFFFFFFF518C8300EB19EB1167110C63FFFFFFFFEF7B05090000FFFFFFFFFFFF00000509B294FFFFFFFF",
    "284267112C1A4911A30054A5FFFFFFFF494A0709CB11CB19AA110B1A69112C1A28096D22E7088D2208098D22491169116D224911AE2249118E228A116D22CB114C220C1A2C226D1A8E22EF2A712B7233D3337233B233AE22CE22EB192809891165000B1A45002B1A6500AA19E7082811EB19860089110809481169112811280968110709691128110809A91965000B1A6500AA11E708C600EB1945002B1A86002811691165000B1A650069114809A600CA11860069118911A60069116909C6086811070948114809",
    "C70848114809280907092809481108092809070948094811A60848096911C60048112811E7082C1AC708CA198911691108094C22AA19C7084C22E7086911EA1189118600AD22A6006911EB19AA1165008D22280908090B1A891148118911EB1928090B1A07092B1A6911EA114C1ACA19EB196911EE22C7000F23302BAE222C1AEF220F2B302BAE222F2B2F2BCD22F702F702F702F702F702BDC6BDC6F702F702F702F702F702F702F7027DB6DC95BFF71ED79823F702F702F702F7021CA6FFFFFFFFFFFFFFFFDA64",
    "F702F702F7024811AE222C1A2C1A4C1A2C1A0C1A2C1AEB190000FFFFFFFFFFFF0000410099CEFFFFFFFF8631C9112C1A4C1A46090C63FFFFFFFFEF7B26090000FFFFFFFFFFFF00004100BAD6DEF7DEF7200000000000000000003084FFFFFFFF8E7306098A110B1A69112C1A49114C1A29114C1A49112C1A6911CB116D2208096D2228094D1A69110B1AEB19AA114C1A8A118D228A116D22CB116D220C1A4D222C1A4D224D1AEF2AEF22B233512BF333512B712BAE224C1A2C1AA6000B1A4500A919AA11A600CA19",
    "86008911070968114809481107098911C608AA1107090809AA1965002B1A6500AA110709A600EB194500EB19C6002809AA118600AA11E70848116911E708691148090709070948096811E7088911C7082809A9196500AA110709A600EB1965000809CA1945008911480965000B1AA60065006C228600AA19CA11481128096C226500EA192B1A69110709EB1989112811EA194809EA190709EB194811A91148096C228600EA19EA19070948112B1A0809E7088D222809EB190B1AEB19EB1189110B1AEA112B1A8600",
    "F333553C3434D644343C753C343C963C553C553CB233F702F702F702F702F702BDC67EE7F702F702F702F702F702F702F7027B85FFFFFFFFFFFFFFFF7B855813F702F70258131ED7FFFFFFFF1CA6F702F702F702F702CA116D228D1A2C1ACE22CA11CE22AA11CE220000FFFFFFFFFFFF00000000BEF7FFFFFFFF41080B12CA114C1AE6080C63FFFFFFFFEF7B05090000FFFFFFFFFFFF00000000BEF7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7BC508CA11AA11AA11CA11AA11EB11AA11CB11CB19AA11",
    "0B1A49116D228A11CA11CB1969112C1A28098D220809AD2228096D228A110C1AEB19CB194C1AAA116D1ACB114C1A0C1A2C1A4C1A4C220C1A8D220C1A502BAF22B233512BB233722BAF22712B2C1A4C22EB11E708CA11C60889112809481148096811E708CA19A600CA110709E708EA1945002B1A860069116911A6000B1A86008911480907096911E7084811480928112809480989118600AA11E708E708EA1945008911481165002B1A8600C708EB194500891148094500EB190709A600AA110709EA1989116911",
    "8911CA114811CA1969112B22691169116911EB19C7080B1A8911E7080B1A6911E708CA11EB1965006C22480989116911EA190809EA19481189112B226909EB196C1AAA194809EE2A6911CA11EB190B122B1ACD2A2B1AAA19AA114C222C1AA9192B1A4C228C1AF702F702F702F702F7027B85FFFF5813F702F702F702F702F702F702F7029823DA647EE7FFFFFFFF1ED7F702F702F7029823BFF71ED7F702F702F702F702F7020A126D22AE220C1AEF22CB11EF22AA11CE220000FFFFFFFFFFFF00000000FFFFFFFF",
    "FFFF00004C1A8A11AD22C5080C63FFFFFFFFEF7BE5080000FFFFFFFFFFFF00000000BEF7FFFFFFFF79CE79CE79CE79CE79CE79CE79CE75AD4942C6002B1A6911EB198A11CA19CA11CA19CA11AA11AA11CB198A110B1AEB1969112B1A28094C1A28092C1A6911CB11CB1969114C222809AD2208098D2249112C1ACB11EB192C1ACA112C1ACB190C1A2C1A0C1A2C1A4C22EB11AE228A11EF2A0C1A0C1A912B8E22B233302B9233512BEF2AEE220B1A0B1A8911681148098911A600EA19A60089114811A6000B1A4500",
    "0B1AC7082809CA11A600AA190709681169112811070969118911C6088911691186000B1A65002809AA1145000B1AC708A6000B1A860069116911860089114809070928098911A91969118911AA1928092B1A080948094C22E7086911AA110B1A4500AD2A07092811EA19CA11A6000B2289116911A91969110B1AC708CA19CA11EA1986006C224811CA110B1A2B1AEA19AA116C2228092F2BA6002B1A6C22EB11EA1989116911AD22EA196C226911CE2AEA192B22A911D302F702F702F702F702F933FFFF3B75F702",
    "F702F702F702F702F702F702F702F702F7027B85BFF7BFF79823F702F702F702F702F702F702F702F702F702140B0F23922B302BCF220F2B4C1ACE222C1AAE220000FFFFFFFFFFFF000020007DEFFFFFFFFFC3188C1ACB11EE22C5080C63FFFFFFFFEF7BC5080000FFFFFFFFFFFF00002100DBDEFFFFFFFF8210210062002100620021006200420089112809EB1989118911EB1949112C1A28092C1A49110B1A8911EB19EB118A11CA11AA11CA19CB11EB198A110B1A49112C1A08094C1A4911EB19AA118A114C1A",
    "2809AD2228096D2269112C1AEB19EB192C1ACB190B1A0B1A0B1A2C1A0B1AEB11CE22CB192C1A4D22AA11EE228A11EF2A2C1ACF22302BF02A9233302B512B302BCE2A4C1A8D2228092B1AC7082809A91165002B1A860089116911C708CA19E708691169116811E7082809A9198600CA112809A6002B1A45008911891165000B1AC7082809A911C608691169112811070969116911681189112B228600AD22C70868112B1A6911C608AD22280986008C224809AA1948110B1A4811CA1928096C22A60089114C220809",
    "07094C228911A6008C224809EA1989112C1A0B1AEA19CA11AA11AD2207094C222C1A0B1ACA11CA198911CD222B1A6C2289112F2BA9118C2248112F2B6811700AF702F702F702F702F7027EE77EE75813F702F702F702F702F702F702F702F702F702F702F7029823F702F702F702F702F702F702F702F702F702F702B112EE225434753CB63C753C1434F3331023F2330000FFFFFFFFFFFF0000820058C6FFFFFFFF8A52C911CB11EE2AE6080C63FFFFFFFFEF7BC5080000FFFFFFFFFFFF0000A408F39CFFFFFFFF",
    "AA5284006C22C7086C22E7082C1A4911CA11AA1128092C1AE7086D22C7086C2208090B1A69118A11EB1149116C228911CA19CB118911EB198911CA19AA11AA19EB11EB198A110B1A28094C2208096C224911EB19CA1169116C222809AD2228094C22AA11EB192C1ACA190B1AEB19EB192C1A4C1A8A118D22CA110C1A4C228A11EE226911AE22EB112C1A8D22EB19AE228D1AEF2A1023502B0F23B233EF22502BAE22EB19CE220709EB1948090709CA19C70869116911481128116911A9198600EB190709C7082B1A",
    "4500A91169118600EA19E708481169110709481128098911A91948098C2A45008C2248114811EB19CA19A600AD2A07090B2269118911A91168118911EA196911E708CD2A860089190B1AA91986006C2248098911891969112B1A6911CA198D222B2248098D2AEB19CA11CA190B1ACA118C22AA1969112B1A0E2B28114C1A0B1A6C22CA190B1AAD2A0B1A2B222B1A4D1AF702F702F702F702F7029A54FFFF1CA6F702F702F702F702F702F702F702F702F702F702F702F702F702F702DA643944F702F702F702F702",
    "F702F702AA090709CA116911AA11EE2ACE22743C3534D63C0000FFFFFFFFFFFF000067093084FFFFFFFF17BE410089092B1A410034A5FFFFFFFFEF7B05090000DEF7FFFFFFFFC31868118631FFFFFFFFBAD6610867110701681162004200060928094C22E7086C22E7082C1A2809CA11AA1128092C1AE7086D22E708AA192C1A08096C22E7084C226911EA19CA118A11EB198911CA19CA11AA11EB11CA1989112B1A08096C2208094C1A69118A112C1A28098D2208096C228A11EB192C1AAA11EB190B1A0B1ACB11",
    "2C1A8A116D1ACB19CB116D224911CE2269116C222C1AEB19AD22CB116D224C1A4C222C1AAD220B1AEF22CE22AE229133AE229133EF22CF220F2B0B1A4C222C1ACA1169116911A919A6000B1A0709C7082B226500A9198911A600CA192809481148118911A91928098C22C608CA192B22E708EA190B1AC6086C22891189198911CA1948110B2207094C22481107096C22481107092B22CA19C6086C2248092B220709AA19EA19EA19C708EE22EB192809AD2A89118D2289114C226911EE2AA91128110E2B69118D22",
    "0B1AAA116C2228112B1A4C222B1A0A222B1AAD2AA919E919D402F702F702F702F702F7027DB6FFFFDA64F702F702F702F702F702F702F702F702F702F702F702F702DA64FFFFDC95F702F702F702F702F7029402471148114811CA1907092B1AE7088911AA1189110000FFFFFFFFFFFF0000B12B0421BEF7FFFFFFFFB6B52421A63118C6FFFFFFFFFFFFEF7B2609620099CEFFFFFFFFFFFFC31863001084FFFFFFFFDAD6694A6108C73975AD38C6AE7384000B1A48110B1A89118A11EA1928094C22E7086C220709",
    "0B1A6911E7088D22E708EB19891169112C1A08096C2207092C226911CA19CA118911EA198911CA19CA11CA19CA11EB1948094C2208094C1A6911AA110B1A0809AD2207096C228A1169116C228A110B1A0B1AEB19CA112C1AAA112C1A0B1A8A11AD224909AD22AA11EB198D2289116D22EB112C222C1A2C1AEB198D222C1A0B1ACE228A11CE222C1A4D1A302B6D1A0F2B502BEF22302B302B0F2B4C1AEE2A0B1A8911AD22C6088911CA11A600CA1948114811CA19CA190A2207098D22281169114C22281169116C22",
    "E7080B22CA11CA19A919E7084C2289196811AA116C2245004C22CA114811EA198911EA1968118911CA114C2265000E2B2B1AA9112B22EB196C1AEA192B1A89110E2BCA1148118C220B1AEB192B22EE2A6C22CA112B228D22EA19EA19EA19EE2A4811AD2AEA196C22CD09F702F702F702F702F70258131ED7FFFF1CA65813F702F702F702F702F702F702F702F7025813DC95FFFF1ED75813F702F702F702F702F702EC11E608C9190709EA19480968110B1AE7088911CA110000FFFFFFFFFFFF0000F3334A1A4C63",
    "FFFFFFFFFFFFFFFFFFFFFFFF9DEFFFFFFFFFEF7B4611A811494A5CE7DEF7DEF7C631681162008E73BEF7FFFFFFFFFFFFFFFFFFFFDEF7518CA400CA11CA198911CA196911EB192809EB196911CA11AA1948092B1AE708AA11CA1908094C22C7088D220709EB198A1148114C1A07096C2208090B1A8A11AA19EB196911CA19AA11CA19CA11EA1969112B1A08094C1A6911AA110B1A0709AD22080949118D2228094C22AA11EB190B1ACA19CA112B1AEB19CB114C224911AD2269110B1A4C1A6911AD2289112C1A2C1A",
    "EB190B1A4C1A0B1A0B1A8D228911CE220B1AEB19CE22AA11EB19CE22EB196D228D228D226D220F23EF2A8E229133AE22CE22502B2C1A4F336C1AAD2ACA198D22A919A919CA194B228600ED2A0709A919EA19EA1928112B2289110B22CA192809CD2AC608EA190B1AA91907096C2248110B226911CA114B22E708EA19AD220B1AA911CD2AEB118C2289112B1A2B1A8D2268110B1A0E2B8911CD2A69114C1AEA198C222B1AAD2A68118D2A4C22CA192B222B226C2227110E2B0611B30AF702F702F702F702F7025813",
    "7DB6FFFF7EE73B755813F702F702F702F70258133B757EE7FFFF7DB65813F702F702F702F702F7027202A911C608CA1928096911EB110709A9118911CA1165004C2200000000000069116911E708E508494A99CEDEF7DEF7BEF795AD863155AD17BECB522B1ACE22A911210000000000E6004C226911E50882100B5B34A5B6B514A5EB5A82108300AA19CA116911CA116911CA198911CA19AA11CA196911CA194811EB19EB1948090B1A28090B1A69118A11EB19E7086C22E7084C1A481189110B1A08098D22E708",
    "2C226911AA190B1A69110B1A6911CA19CA11CA1989112B1A28092B1A89116911CE2A2811AA112B1A08098D2228090B1AEB11AA112B1AAA11EA190B1A0B1A89116C2269112C1A0B1A4911AD2249112C1A2C1AAA114C22EB190B1A2B1A2C228A118D220B1A8A11EE2A2C1A8A11CE22CB112C1A8D220B1A2C1A6D1A6D22CB11CE224C222C1A2F2BAE22AE229133AE2270330F2B7033AE222F2B0F2B4C228D220E2B6911AD2AEB192B2269114C22EA19EA1948110B1A4B22A6006C22CA11CA1968114C2289112B22E708",
    "CE2A2B1ACA194C224C228C224811CD2A89110E2BCA19AA19CD2AEA194C226811CD2AA9110E2BA9192B1AE708EE2AA919AD2A27110E2B89190B226C22CA192B220A122C1AD602F702F702F702F702F702F7029A547EE7FFFFFFFF7EE7BDC6BDC61ED7FFFFFFFF7EE79A54F702F702F702F702F702F702D6022809A911E608AA19891169112809CA11891186008D22E7082B226500A91928096911EA19E7084C22440062000000000000006611D12B4611E4084B1A702B912BCE22B1330B1ACE2269110B1AAA116911",
    "0B128400C4084100C308C5006809CA1108092B1AE7082B224809CA19AA116911EA192809EA196911CA19AA1169118911AA11CA198911CA1928090B1A6911EA11AA1948094C22E7086C222809CA11CA1908098D22E7084C224911AA190B1A48110B1A6911EA19CA11CA1989110B1A691189114C2228092B1AAA1128096C22E7086C22691189114C1A48110B1ACA11EB19CA110B1A89112C1AEB196911AD2248112B1A2C1A49118D228A11EB192C1AEB19EB194C1A0B1A69112C1A4C226911AD22CB11CA19AD22CA11",
    "2B1A6C1A2C1AEB198D22CE2A0B1AEE2AAE2AEB192F2B6C228D22CE22CE2A6D22AE220F2BEE2A6D227033EE2AAD227033EE2A50338D222F2BCE22CE2A2C1A0E2B4C22CA19CD2A0B1A2B2289110E2BEA19AD2A8911AD2A8D2289114C226C226C222B1A2B220B1ACD2A0B1A2B220A1AAD222B226C22EA212B1AA9192B1ACA1989112E2B69114C22EA198C2289198D220B22C9194B228B09F702F702F702F702F702F702F702F702F9337B85BDC6BDC6BDC6BDC67B85F933F702F702F702F702F702F702F702F702AC09",
    "0709C91107090A1A86002B1AE708E7086C22C608EA1965006C2228092B1ACA1189114B220709AD22C6082B1A0709EA19CA1948099033B133B53CB63C1334133CEF2291336D1A0F2B8D224C1AAD224809AD22E7084C2248118911CA19E7086C22C7084C220809AA11AA1108094C22C7082B222809CA19CA1148110B1A07098911EA194811EA196911CA19AA11CA198911EA194811EB19AA1969110B1AE7086C222809CA19CA1908096C22C7084C22691169112C1A08094C226911CA19EB19AA11AA19CA11CA194811",
    "2B1A8911AA110B22E7088D222809AA112C1A08096C226911CA190B1AAA11EA190B1AEB1989114C2269110B1A2B1A2809AD226911CA196C2289110B1A2B1ACA19EB190B1A4C2269114C222B1A6911AD22CA11AA110E2BEB19CD2A2C1A0E2BEB19AD2A8D226C222B1A4F3389110E2B8D22CD2A0B1A0E2B6C22AD2A4C1A4F330B1AAD2A2F2B8D22AD2A4C224F336D22EE2A6D2250338D228D222F2B4F2B0F2B0F2B70334F2B70336D22B133CE222F2B8D220E2B4F338D222F2B0B1A90336C222E2B2B22EE2ACD2AAD22",
    "28114B222B22EA19EA216C224B2289116C220B2289118C2AAA192A228911AC09F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702CD094811E6080A226500AD2265008911CA19E7088919E7088C2207092B1A07096911691107090B22E7086C2228090B1A681189112B2228094C2228098911EE2AB1331645743CD233302BEF222F2B6D1A4F2B0C1AEE2AEB19CA11EB1908096C22E7084C2208098911CA19E7086C22C7084C2208098911EA19E7086C22",
    "C7082B22CA11C7082B222809CA19CA1148110B1A2809EA198911CA19AA1189116911CA11AA1989110B1A08092B224811AA11EA19E7088D22E7080B1AAA1128116C22E7082B22891148110B1AAA11CA19CA19CA198911EB19EA1948114C224811EA19EB1907098D222809CA194C1A28092B22CA11CA19EA19EA19AA19EB192B2248118D22CA196911AD226911EB19AD22AA11CA192B1AEB19CA190B1A4C2269116C222C222B226C22AD2AAA19EE2A4C226C222C22CE2A2B224C22CD228D22CA190E2B4C1A6C226C22",
    "0E2B0B1AAD2A6C222F33CA192C220E2B6C226C224C222F332B1ACE2A4C22B0336C222F2BEE2A4F330E2B6C229033CE224F338D222F334F2B2F33CE2AEE2A4F334F2B2F2BEE2A2F334F2B70332F2BCE2A6C222B22C9194B222B22A9198C2A2B1A8919ED2A89194B2289116919881107110D12D602F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702D6028B092711C911A6008C224500EA19891148112811A9112B22E7088911A600891168118911CA192809A919E708EA19",
    "8911EA11CA1969110B1A4811EA19C91989110B1A07098911AD2A543CB644F2339133CE22502BAE22CE22CE2A4C1ACE2A89114C1A6911AA11EB1908094C22C7082B1A48116911EB19C7088C22C7080B1A691108090E2B080969110B1AE7086C22C708EA19891148112B1AE7080B1A8911AA19CA196911AA198911CA198911EA1928110B1A891189110B22E7086C222809AA110B1AE7088C22691107096C222809CA190B1A6911EB19AA11EA19AA11EB1969110B1AEA1928096C222811AA112C22E7086C2289118911",
    "4C226911EA190B1ACA1989112B1AEB1969118D22AA1128096C22EB1969118C22CA11AA192B1A2C224C224C22AD222B222B1A0E2BAA116C22AD222C220B1A0E2BEB198C2A4C22CD2AEB196C22CD2A4C222B22CE2A8D220B1AAD2A6C22EE2AEB19AD2ACD2A8D2A2C228D222F2B4C220E2BCD226F33CD22CE2ACE2A9033AD220E2BCE2A4F334F336C224F338D22D13B8C224F33AD2A6F336F330E2B6F338C22D13BA9196C226C224811AD2AA9196C2A2B1AEA19CA192B224B2207092B22C608C919471128099302F702",
    "F702F702F702F702F702F702F702F702F702F702F702F702F702F702F702720269094811E6080A22C60889118911481127110B1AA919E70848116811EA19CA19EA19EA19EA19EA19CA11A91989118911A919EA11CA1989118911CA19A911EA198911CA19A919280948110709CA19B133D544543C912B7033EF22EE2A8D22AE228D228D226C22AA110B1A2811EB1989118911EA19E7082B220809EA19891928094C22C708CA112B22C7086C22E708AA11EA19C7088C22C7080B1A891128096C22C7082B2269118911",
    "0B1A2809EA198911CA19AA11AA198911EA19CA1969112B2208092B1AA9190809CD2AA9110809AD22E7080B1AEB1948116C224811CA190B1A8911CA19EA19EA1969112B228911CA112B2207094C22891128098D222809CA194C226911EA190B1ACA19AA192B1A0B228911EB194C2248112C228D2248110E330B1A6C222B22CD2ACA198C2A4C22CD2A69110E2BEB194C224C22EE2A8911EE2A4C22AD2ACA190E2B2B228C2A0B1A2E330B1ACA19CD2A6C22EE2AAA19EE2A8C22EE2A6C222F33EE2AEE2ACD2AAD2AD03B",
    "6C222F338D2270330E2BAD2A0E2B0E2B6F33EE2A0E33CD2A4F334F334F33EE2A2E332F337033D13B8911CA196C22EA192B228911CD2A07110E2B2811CA19C91186008C2265006C22E60868114811CC09B402F702F702F702F702F702F702F702F702F702F702F702F702B4028C096711E608C91128098919EA19E7086811CA196811070989190B1A8911A919070989192809891989114811EA1948110B1A89118919CA1169110B1A0709EA19CA114811CA19070948112809E7086811A600681107092F33953C7444",
    "D1330F2B2F2BEE22EE2AAD226D228D224C1A4C22EA19CA196911AA198911CA11CA194911EA190709EA198919C7080B1A691189110B1AC7086C22E708AA11CA19C7088C22C708EA198911E7088C22C7080B1A891148114C22E708EA19AA118911EA196911CA19CA11CA1948110B1A891907092B22891169114C22E7084C22891108098D22E708EA190B1A28112B228911CA19EA19AA19CA19EA19EA1948112B1AEA1908098C22691169118C220809CA194C224811EA192B1AAA19CA19EA192B22AA196C226C220B22",
    "EB19EE2AAA194C22AD2AEA194B22AD222B222B226C228D220B222B22AD2A4C22EA19ED2A4C224C222B220E2BEA198D2A2B222F338C2AEB19AD2A8C220E2BCA199033AD222E336C220E332F2BEE2A8C2A0E2B0E330E2BEE328C22903BAD2A90332B224F33EE2A6F332F33AD2A4F33AD2AF13BAD2AB03B4B228911AD2A07116C224B220711ED2A0709CD2AC9198911C919C6084B22A6004B220709AA194711A911A8118A092F0A7202F702F702F702F702F702F70272020F0A89092A1AC608C911E6086919EA190711",
    "C91169116911A6002B22CA11481169118500EA190709EA1969116811EA19C6080A1A07090A22CA1148110A1AE7086C222809A919A91107092B220709E70889116500C919860068112709C608A919EB1994449444D23B7033CE222F2B6D22EE2AAD226D22AD22EB192B228911CA19CA11691189118911CA198911A91969118911CA192811EA1969118911EA19E7082B222811AA11EA19C7086C22E708AA19EA19C7088C22E708AA19EB19E7086C220709A9190B1A08090B226911AA19CA198911EA198911CA198919",
    "CA11EA1907094C22891928096C22E708EB19EB19E7086C22691189116C222811EA190B1A8911CA190B1ACA1969112B22CA1948116C22691169118C22281148114B22AA11CA198C2A4C22CA198C2A2B1A8C2A8911EE2ACA192B224C22AD2A48110E330B1A4B22EB190E2BA9198C2A4B22CD2ACA198C2A8D226C22EA198C2AAD2AEB19AD2A2B220E2B0B226C220E2B2E2BAD2A8D220E33EE2A4F33EA196F33AD2A4F338D2ACD2A4F33ED2A2F336C222E332E2B6F33ED2AED32CD2A4F332F334F33CD2A0E33EE320E2B",
    "0711ED2A28110A1A2B220A1AEA21CA192B222811A91968114811EA190709EA198911A9114911A911A911A911681188110711671168116711471147112711471148112A12C608A9196811681189118911891186008C22E7086811070927116C2227098C222809EA19CA1148118C22E7084B22C7088911EA1927096C22A6004B2289116811EA1986000711481145000B224500A919C608A608A9194500EA21650028112B1AD233F54C91339033EE2AAD2A2F2B0B1A0E2B2C1A6C22AD2289114B224811CA19CA194811",
    "EA194811EA19AA194811CA19AA1189196911A911A919A911CA192809EA1969118911EA19E7082B22481169112B22C7084C22481108096C22C7080B22891128096C22E708CA19EA19E708CA19EA196811CA19CA19CA198911EA19A91989112B222811EB19EA19E7086C22691128118C220709AA194C220709EA210B1A6911EA19EA19AA1989190B1ACA1948114C22EA19CA19CA190E330709AD2A2B220B22EA19ED2A89116C2A6C224C22CA198C224C224B22EA19ED2AEA192B226C22AD2A8911AD2A6C228C2A8911",
    "0E332B228C2ACA190E338C2AEA19AD2A6C22903B0B220E33AD2A2E33CD2A8C2AED2A2F33ED2AAD2AEE32ED2A903B4B222E336C22903BED2A0E33CD2ACD2A903BCD2A903B2B226F3BAD2AB03B0E2B903BCA194B2AEA194B220711CD2A0A220A1AEA190709EA1948118911C91969118919EA196911EA198911C919CA1168112B22E708C9198911A91968114811A9192709C91927092709EA19E708EA19C70827096C224500EA19C7080B1ACA19E7082B226500EA1986002709A911C6088C2AE7084B22681107090A1A",
    "E708AC2AE7088911A91148118C2265008500EA194500EA196500E708691145002B2245006811070965000A2245004B22B13BB444333CCE2A90336D22AD2A0E2BEB192F33EB196C224C2248116C220709EA19CA19C708CA19CA1128110A1A0809CA19CA116911A9196911CA19A91189196811AA11A9196911EA192811EA19CA1907094B220709CA19EA19C6086C22280948116C22C7082B224B22E708EA19EB1907092B226911A9190B1A6811CA19CA19A9198919EA19EA1928114B22A91928118C22481169118C22",
    "0709CA196C220709EA192B1A48110A220B1A8919CA194C228C220A222B22AC2AEA190B228C224B2269110E33AA190B224C22AC2A4811AD2A2B224B22CA19CD2A0A1A4B220B22AD2A0B220B228C2A8C2A2B22EA19ED2A2B226C2A8911AD2A4F336C228C2A0E2B2E336C22EE324C22B03B4C22EE32AD2A0E334E338C2ACD2AAD2A4F33EE2AED32AD2A2E330E2B6F33AD2A2E338C2A6F33ED326F330E330E330E334B222B22A919AC2A4811A9194B222809EA190709A919891948110A22891168112B1A28114B226811",
    "0A22891127114B22E7080A222709A919681107090A1AA6002B228500C6086C224500EA19891165000A1A27098C22A600E7086811A6006C22C6082B1A681148110B22A6002B22C608CA19EA1928094B2286002B1A89114811EA218500CA19C6084500EA196500681127116500EA194500A911C608A600C9194500EA198600E708A919C608123C9444123C70338D2290332C22CD2AAD2AEB192F33CA196C22EB1907098C2AE70807096C22E708CA19CA19E7084B220709A919EA1907090A224811A919CA192811CA19",
    "A911CA196911A919A919A911EA192809EA19A91928094B222811A9110B22C708EA194C22C7082B22A919E7088C2A280969116C220709EA190B1A4811EA19CA19A919A919EA19CA1969112B22A91948114C22691169118C22281189116C220709CA190E334811AC2A4B228C2A6911CD2AEA196C2ACA19AD2AEA194B220B1AAD2A69114C226C224B2269110E33EA192B22EA190E3368116C2A4B22CD2A6911AC2A4C22AD2AA9198C2A0E2B4C220E33EA198F3BAD2AAD2A8C2ACD2A2E33CD2A6C2ACD2A4F33CD2A2E33",
    "0B228F3B8C226F3B6C22ED2A0E33EE326F336C222E336C226F3B0E2B4F33CD2A0E33CD2A2E334F3389114B22C919EA21AC2A27114B22E708EA1928096811CA19E7084B22691189112B1AE7086C2207092B228911E7080A1A07092B22E708A9118911A6086B224500AC22A6084500AC2AA6002709891168110A224500CA196811C9112B2227096B220709EA19A91969116C2207092B1A48114811EA1928096B2227118911C91969114B22A6086811A91945008911E708E708891945008911C608070989196500A919",
    "A608280968196500CA1985004811CA192F2BF54CB13B6F332F336C224F332B22AD2A8D2ACA192E3389110B1AAD2AE708CA19EA19E7088C2AE70869110B1AC6084B22280968114B22C7080A22891148112B220709CA19EA192811CA19A911A91989118911A919A911CA192811EA19CA19E708CA190A22E7084B228919E7088C2A070968116C22E708CA192B22E7082B22CA1948112B22A9118919EA19A911CA19A919EA19CA1969112B22A91989196C222B2269112E330B2269116C22AD2A69114B22AC2AEA19CA19",
    "ED32A9194B222B228C2AA9196C2A2B226C2ACA194C226C222B22CA196C228C2AA9192B226C228C2A6911AD2A6C22ED32EA19ED320E33ED2A8C2A4C226F336C224E33EA192E330E2B0E338C2A8C2A0E330E2BCD2A8C2ACD32ED2A6F3BAD2A4E330B226F3B8C2AB03BAC2A2E33AD2A0E332E330E336F338C2AA9190A222B224811AC2AC6082B22E708EA19481168112B22A6008C222809CA190A1A07098C2AA6082B2269114811EA1907090A22E7086811A91985008C2A45000A1A2B226500C9196811A9114811A600",
    "4B2248110A1A4811E708891985008919E70848116819E708CA1948110A1AEA196911A919E708EA19EA19A9118919E7082711070928090711E70828096819C6084811C60848114811C6086811A60848114811C7086811A60048114811C60889198500C911D03B94445244EE2A4F33EE326C222E330B22AD228D2ACA190E2B4C2207096C22281189112B22C6086C22281107096C22C6080A228911E7088C2AE70889192B22C7082B22691148112B220709A919EA192811CA19A9118919A9198911CA196911EA19CA19",
    "28110B1A891928094B22481169114B22E708CA192B22E7080B22EA19E7084B22A91928114B22891168112B2289118919EA21CA198C2ACA19AC2AEA210A22CA19ED3289192B226C228C2A48110E33CA19EA212B22ED3248116C2A6C224B226911ED32EA192B22A9190E33A9196B2AEA19CD2AA9194B2A0B22AC2A6C224B22ED2AED2A4E33EA19CD2AED2AED326C228C2AAD2A2E33CD322B222E336C22B03B0B220E336C222E330E33CD2AAD2A8C2A0E330E332E33ED2ACD32CD2A0E330E338F3BCD2A8F3B8C2AD03B",
    "CA196B2AEA19CA19A91107090A22E7080A22070989112B22C6088C2A0709EA19A91928098C2AC608EA1928116911A9194811C91948112811EA19A600EA19E7086811A91968118919C60889112B2228096811C6086811A919A911EA198911EA19EA19CA19A9196811481148118911C919A911891989196811A919CA19A9196811070968192709650048112709E7080711E70848112809E7080709E708481128090711E708E708481128090711E708070948110709271128119033B44CF13B6F33CD2A0E2BEE324C22",
    "CD2A2B222B22CD2A2B220B1A4B22E7082B22891948114B22E708EA19CA19C6086C22070948114B22A6082B228911E7088C2AE70868114B22C608EA21A91907094B22481189190B2248116811EA198911A919A9198911CA198911CA1989198911EA214811A9190B220711EA190A22E7080B220A22E7082B22EA19E7082B222B2289198C2A4B22EA190A22CD2A48118C2A6B224B22EA214B2A2B224B2ACA198C2A0B224B2AA919CD2AEA210B222B22CD2AA9190B22AC2A4B22A9194B22CD2ACA192B224B22ED32EA19",
    "8C2AAC2A6F3BCA19ED324B22ED2A2E33CA190E336C224F336C228C2A8C2A2E33AD2AED322B22ED2AED32ED2A0E330A224E336C22AF3B6C224E334B220E33ED2A4E332E334E3BAD2A6F3B6C226F3B6C2248110A22A6002B22E7084811EA19E7082B22C608A9112B22E708AC2A07090A1A891127096B220709EA194811A911A9194811681168112809CA1907096811A9114811A919C6084B22271107094811A9110A1A891968114819C708C6080709C708481928092711C919EA194B22EA1907112711C6082B224B1A",
    "8911281185006811270907114811A60007114811850068190709271148118500681907092711481186004811070927112811A6084819281127110711C60848194811EA21D44C5344903B2E33EE328D2AAD2AED32EA19AD22AD2A0B1A8C2ACA19A9190B2207090A1ACA1928114B22070989110A22A6084B224811E7088C2AC60889112B22C6084B226911C708CD2AC60848116C22A608EA21AC2AE70889192B22C7080A22EA19E7080A22CA196911A919CA19C9194811EA190A2207090B1A0A22E7082B22EA21C608",
    "6B220A22A6080E33EA21A9192B22ED32C608AD32AC2AEA21E7084E3BA9190A220B22ED320709ED320B222B22CA19CD32EA198C2AC919AC2ACA19ED324811CD2A0A22AC2A4811ED322B222B22EA196F330E33EA198C2A4E330D338C22EA21AD2A4E33AC2A6C226B2AED2A8F3BEA210D33A919B03BCD2AED326B228C2A2E332E33CD2A8C2A6B2AED320D336F334E33CD2A0E332B224E338C2A8F3B6F332E338F3B27114E3307116C22C6086811CA19E7084B22C608A9110A2228118C2A07092B1AA91928092A222809",
    "CA192811CA19681968116811681168116819C91907112711EA19A919E70828110A22A91107110709E708C919EA19C9190A1A69110A22CA19C919EA192711C91907096819A91948112B2248118919891107114B22A600481148118600A919E7088600A9198600681948118600A9198600481148118600A919A6004811681185008919C6082811681185008819E70827116811A600EA21B03BB44CF1430E332E33EE2A8D2AED328C226C226C2A6C228C2AEA19CA19A919A911EA192811EA19EA2127110A2269196811",
    "2B22E708CA19EA19C6084B22681107096C2AE70869114B22C6080A22CA19C708EA216C22C608EA190A22E7082B22A91928112B22891168110A2289118919CA19A919C919A919CA19C91968110A1AA9194811CD2AEA21CA196C2A4B2A89194B22AC3228118C2ACD2AEA19A9196C2A8C2A89192B22CD2A8919EA19AD2A2B22A9194B228C2AA9190B224B228C2A89196C2A2B22AC2A89198C2A4B22CD322B22ED328C2A4E330A22CD2A0D2B6B224E33A9190E338C2A2E338C2A4B2ACD2A0D33CD2AAC2A4B2AAC2A2E33",
    "AD2A4E3BCA194E3B4B228F3BAC2A2E338C2ACD32AD2A2E33CD2A8F3BAC2AAF3B8C2A6F3B0D338F3B8500AC2A45002B22E7088911A91907096C220709CA19EA1968112B2207090A1AC919891189196811A9114811CA196811681148112811EA1948118911891107112709EA19EA19C7084811891168114B224811C91968110709A919A600A919481148112B2227094B22A9114811EA1965000A22891189114B228600A919C608C608C9194500A91948114500A919C608A608CA194500A919E7088600EA1945008819",
    "07098500EA194500281168116500EA198600071189114500A919E708E708C919E708F043B44CB03B8F3BCD2AAD2A0E338C2A8C2AAD2A0B228C2A6C22EA19CA19A919CA19A9198911C919A911A9196811CA19C9190711CA19A91928110A22681189110A22E708CA19EA21C6082B22A919C60889192B22C608EA19EA19C6084B22A919E7086B22891107116B2A681128114B22691168112B22891169192B22CA194B2AEA218C2AA9198C2ACA198C2AA9196B222B224B2A2811AC2A0A224B2A68110E3389190A222B22",
    "CD3207118C2A4B224B2A07110D330B222B2269110D33EA210A22CA19ED322B226B22CD2AED32ED322B22AC2AED2A4E33A9190E336C2A2E33CD324B22CD324B228F3B2B22ED320A220D33ED2A0E338C2ACD324B224E33AC2A4E338C2ACD2AAC2ACD322E330D332E332E33AD2A6F3B8C2A6F3B0E336F3B4E3389194B22A6080A1A0709C919A91907094B2248110A1AEA2189110A1A48118911C919EA192711891148118919A9116811481168114811C91968114811E7082A22C6086500C919A911C919A9114500A919",
    "A6006819C919E7084B2207098919A911A6000A2286008919C9190709AC2AE70889116811C608AC2A65002711481145000A22450007114B2245000709891145000A226500C608A9194500EA21A60085000A224500A919E70865000A2245002711681145000A228500C608CA194500A919070985008C2A6F3BB354F143CD2AEE324F332B22ED32AD2A0B22ED320B224B2A6C222811EA21EA192811EA218919A919C9196911CA21A9196811C919A91989196819A919C9192711EA19C91907090A22EA210711A9190A22",
    "E708EA19EA21C6082B22C919C6084B22A919C6086B22A919C7084B22A919E7084B22EA19C9192B228C2A68114B2AAC2A68118C2A4B220A22A919CD3289194B2A0A226B2A0A222B2AEA216C22EA214B2AEA216C2A0A22EA214B226B2A0A22EA19AC2A2B222B22CA19CD322B226C2A6C224E3B2B22ED320A224E33AC2A8C2A4B222E330D338C2AAC324B222E33AC2A2E33EA210D33AC2A2E330D334B22CD328C2A4E33ED320D33AC2AAC2AAC2A2E33CD2A8F3BAC2A4E3BED32ED324E3BED328F3B2E338F3B8F3BAF3B",
    "0B1A0C22CB194D22CB196D224C2AEB196D220C226D224D2A4C222C224C220C1A0C228D22AA194C228A114C2ACB190C220C22CB192C228A192C226D2209116D2A4D22AA196D2A6A118A19AE2ACB190F330C220C22CE32AA19EE32EB212C220F33EB210F33EB21CB218D2A6A19EE320C224C2A0F338A198D2A8A196A19AD3249196D2ACB2169192F3BEB216919EE3A69192C2A4C2A6919EE3A6A21EB298D326A21CE3AAA21AA21EE3A8A218D322C328A210F438A212C2AAD3A8A21EE3AEB29AA292F43AA296C328D3A",
    "2C32D464156D935C735CD153704B325C4F4B704BF153EE42B053904BAD426F53CD42AD4290538D3A0E4B4F4B8C422E4B2E4BAD422E530E4BCD4A2E4B0E4B0E4B0E4B0E4B2E530E4B2E4B0E53EE4A2F4B2E53CD4A4F4B4E53AD424F4B4E538C426F4B4E4B8C3A6F4B4F4B6C3A4F4BD053AD42B053904B2E4B0E43325CAD3A6F4BD0532E43ED42F1538F4BED3AB04B1154EE3A4F434F43F14BAD3A904B8F438F43CD3AD04B6F436F43EE3AF14B2E3B6F43EE3AF14B2E3B8F430E3B324CD04390438F431144D043903B",
    "B0436F3B324C903BB0434F33D0431144B03BB03B6F3BD03BF13BF1439033D13B2F335244903352446F33F13B9033113CD0333244B03352449033323CB033F13B323CF13B5244323C72449344B344D44C4C22EB210B1A2C22CB196D226D2A2C222C224C224D224D2A4C22AB194D220C22EB216D226A118D2A49116D2AAA19EB194D222911AE2AAB1929110F33AA190C228A194911AE2A4D22AB19CE3229112C22AA192911AE3249114C2ACB194911CE328A198D2A6D2A0B222F3BAA214C2A2C2AAA192F3B6A19EB21",
    "2C2A4919AE328A19CB216D3249192C2A8D3249198D32CB29AA21CD3A6A194C2A2C2A6A21CE3A8A210B2A8D3A6A21CD3AEB29CA21EE3A8A216C326C328A21EE3ACB29EB29EE428A298D326C3AAA290E43CB29EB29904BB053D464356DD153D153325C2F4BF15390530E43115C2F4B0E4BD0536C3AEE4290534C3A2F4B2E4B6C426F53EE4A8C428F53CD42CD4A8F53AD42EE4A6F53AD420E536F532E4BED4A2E530E4BEE4A2E4B2E4B0E4B0E4B2E4B0E4BED4A2E4B2E4BCD422E4B8F53CD4290538F532E4B4F4BB053",
    "8F53AD3A315C0E432E4B8F4BD0538D3AB04B904B4F4B4C32B04B8F4B2E43CD3A11542E430E432F431154CD3A2E436F43D04BCD3A2E3BB0439043EE3A6F43314CD14390432E3B524CB043B0432F3B314CB03B11440E33D043D143D04332440E33F1436F333244B03BD0436F33F143B03B52446F3311444F33F13BD03B113CF13B113CB033324490333244D13B323C323C323C724472449344B34C35559655965DEA194811891189116811C919EA19EA1968110A228911C9190A1AE708C919891127110A1A65006C2A",
    "6500EA1927116500AC2A450068110A1A65000A22A6000A1AC91985002711281165006B2AE708EA19EA192711AC2A2711C9190A22E7084B22E7080709A919A6004B22681189112B22A608A919E708A608C9194500681107118600C9198500A608A919A6000709891945008919E708C608A9196500681148116500A919A6080709A9194500891127118600C919A6000709A9194500A91907116500EA198600E708C9194500E708EA218500CD2AD354D043AF3B0E3B4B2A6F3B4B2A2B222E3BEA214B22CD3227110A22",
    "0A22E7086B2A8919E7088C2A281107118C2A070928118C2AE70868196B22C60807112B22891907112A22891907110A2289194811EA21A9194811EA21EA1948116B2A0A224B2AC9218C2AC9198C32CA194B222B2AC9216B2A0A224B2AC919AC32A9192A222B228C2A2B2AA919AC2A2A22C921EA21AC2AC921C9212B22CC3289190A224B22CD3248118C2A6B224E3B0A22CC328C2A6E3B0A228C2AAC2A0E336B2A8C2A6B2AED320E332B224E3B0A226E3B8C2A0D336B2A8C2AED32ED32ED32ED326B2AED32AC2A0E33",
    "2E330D330E330D33CC2A4E3BCC326F3B2E338F3B8F3BAF3BD0431044724C1455345D51546B2A2711EA194811A9196811EA21C919C9192A2248114B22481168110A1AE708C9196811E7082A224500AC2A65004811EA1945006B2A0709E708891927098C2A6500C608C919E708A9194B22E7082B22E7080709E9218500C919271127092A22E7082B22C91989112A22E70889114819E7080A22E7082711681965006811E70807096819650048114811450048114819C6086811C60828114819860048112711E7086811",
    "A60848116819A60048112711E7086811A60048116819860068112711E7088819A608481168196500891148114500681168196500A9190D3B725472542E3BED324E3B4B2AAC2AED32EA21AC2A8C2A68196B2AA919E7088C2A68190711AC2A271127118C2A071148118C2AE70848118C2A0711C6084B2A8919C6084B2A8919C6082B22C919E708EA21EA21E708AC324B22A9192B2A8C2AC919A919ED3248114B2A0A226B2A89198C2A4B22EA210A224B2A2B22EA214B2AAC2AA9196B2AA919AC2AA9198C32A919AC2A",
    "89198C32E919AC2A89198C32EA210D332B22CD32AC2AED328C2A8C2AAC2AED320D330A22ED32AC2A2E336B22ED320A228F3B4B22ED328C2A6B2ACC32AC2A6F3B8C2A0D336B2ACC32ED322E3B0D334E3BCC320D33ED32ED326E3B0D338F3B6E3BAF3BCF43F043514CD354345DD35C2E3B4811A608A608C608EA196819EA1928114B22C919A9192B22E7084B2268112711EA190709E9216811A6086B2245004B22C608A6086B228500C91907098911C91945000A22891968110A226500A600C9196500C91968192811",
    "2A22E708C919A9190709A919C6084811A9190709EA19A919EA19EA2107094811271107092711A60827116819C6080711071128112711A60827090711C60807096819E708E708071148110711E708070968190709E708071148112711E708070968190709E708271148110711C60827116819E60807096819070927118819C60827114819E70848112711ED32B35CF04B6F3B4E3B6C2AED32CD320A22CD328C2AEA218C2AA91948116B2A681968114B2A481168114B2A071168116B2A07116811AC2AA919C6084B22",
    "A919A6082B22C919C6080A22EA214811C9190D33C6086B2A8C2AA9198919ED32EA214811ED32EA21A919EA210D3BE7084B2A6B2A4B2A2711CC324B22EA2127118C326B2A0A226819CC322B22C919A919CD322B22A9190A228C2ACC2AEA218C2A0D332E33EA19AC32AC2A4E3B2B22AC320A228F3BAC2ACC320A22ED32AC2A6E3B4B228C2A8C2A8C2A6E3BAC2A4E3B0A220D3B6B2A4E3BED324E3BCC2A4E3BAC2A2E3B0D330D338F3B4E3BAF43AF43F0433044B254345D135DD04BC921C608A608A608A608A608A608",
    "C91989194B2227116B2AA91189196B22C6082A2248114811C9190709C919681185006B228600A919A9198600C9212711EA19C608C6088C2AE70888110711C6080A22681168114B226811EA19EA212711EA196811EA190A226811EA19E919EA19A91927112711681928116811481127092711E708C60848192811C6080711E70868192711A600A9192711A60868194811C6084811E70848112711A60068194811A6084811E70827114811A60868194811A6004811270907112711C60868194811A60068194811C608",
    "48112711A60868194811C608271148110711071148112E3B9254314C8F43ED32CD32ED328C2A8C2AAC326B2A6B2A0A22C919EA21A919A9190A228919A9190A226819A919EA21681907112A22E921E7080A22EA21E708EA210A2207110A22CC32A919C9190D3B6819EA218C2A4B2A0711ED324B2A68192B22AC2AC92168190D3BEA21A919EA212D3B0711EA216B2ACD32E9214811CC324B2AC92188190D330A22C92188192E3B0A228C2AEA218F3BCC2AAC2A89194E3BCD32ED32EA21ED32AC2A4E3B4B226B2ACC2A",
    "0D33ED32CC324B2ACC322E3BCD2A4E3B4B222E3B2A226E3BCC2A6E3BED324E3BAC2A4E3BED324E3B4E3B4E3BAF438F3BD0431044924C135D545D51544B2AE708A608A608A608A608A608A608A6088600891189196B2228116B2A681127114B2207090A2227112811EA210709891968118600EA212709681107116811C9214500EA19E92186008819E608EA192A222709A919891986002711071127096811E608271168192709E708E7084811A91968116811EA190A1AA9198919E7086819281186004811E7080711",
    "4811850068192711A6088819C608A6088819E708E70889196500481968116500A919E708C608A919650048196811450089190709A600A919A608071189196500681968116500A9190709A608A919C6080711A919C608A6008919E708E7108919C60827118811A60888194E3B725471548F3BCC320D3BED326B2ACC32AC2A4B2A6B2A2A228919EA21EA19A919A919EA19A919A919C919A919EA216819EA19E9216819E919C92148118C2AE921461141000000000000002100E408C9192A22CC3227118B2A4B2A2A2A",
    "8919CC322A2A89192B228C2AEA218919AC2A4B2AA9190A222A22ED3248112A222B22CC3248114B2A2A22ED320A22CC32AC2A2E3B4B228C2AAC2A4E3B8B2A4B2ACC2AED32CC328C2AAC32AC2A4E3B6B2ACC326B2A0D33ED324E3B4B22ED324B222D3B8C2A6E3BED324E3BCC324E3BCC326E3B2E3B6E3B8F3B8F43D043F043514CF354345D9254ED322711A608A608A608A608A608A608A608A608A608A608A608A9116811AC2A27114B22891107092B22E708EA19271127110A220000000000002100C919E708A919",
    "2711C6086B2AC6084100000000000000271107094811E60888112A228919EA190A22E919A919C919EA19EA21E9196811E921C91927118919C608681988110709A919E708E70888196500481948116500A919C608E708A9194500681927114500271188114500A919E7088500E92165000711A9194500891907096500EA21A600A608EA194500481189114500C921070965000A22A600A608EA2165000711C919450068190A228500E708EA1965002711C9194500681989194500891989196B2AB25C51540D3B6E43",
    "0D334B2AED3AAC2A2B2ACC324B2A88192A22EA1968190A22EA214819EA21EA190711A919EA21891989190A222A220A2AA81962086D6BDAD6DEF7DEF7DEF75CE730848210E921C9198B32A9196B2AC9198C32A9198B2AEA216B32A9198C2AE9214B2AEA218C2A68196B32A9198C2AC9214B2AEA216B2A2B224B2ACC2AED328C2AAC2ACC32CC32CC326B2AEC32AC2A2D334B2AAC32AC2A4E3B6B2A0D3BEA210D3BCC324E3B8C2A0D3B0A224E3BAC2A4E3BED324E3BED322D3BCC326E3B0D336E3B6E3B8F43CF43D043",
    "514CD254345DD25C4E3B6819A608A608A608A608A608A608A608A608A608A608A608A608A608A608EA194811CC2A0709EA19881127114B22A608EA19481107090000FFFFFFFFFFFF79CE4100A608C919E921650068116200D7BDFFFFFFFFFFFF0000EA19A9196811681127112711281107112711A60868194811C608A9190709A919EA1907094B22C91968110A22C608481968114500C921C608A608C9194500481948114500E921A6008600EA21450045000A226500C608C91945008819071145000A2286008500",
    "0A2245000711A9194500A919070945002A22860065002A224500C608EA194500481189194500C921070945008819EA214500481189194500881968114500C92107114500EA21E70885006E4372543054F04BED328C324E3B8B2A2A2A0D3B6B2AA9196B2AC91927114B2A0A2207110A226B2A68116819AC32C9190A2A6B2AE8212421DAD6FFFFFFFFFFFFFFFFFFFFFFFFFFFFFBDE2521292A4B220A228919CC32E9192A2AA921CC3268114B2AC921CC3268118B32EA21ED320A22C9214B2A6B2A0A22A919AC320D33",
    "0D332A22EC32EC32E41000000000AC2A6E3B4B22CC320A224E3BED322E3BEA21ED326B2A6E3BED320D3B4B220D3B6B2A6E3BAC2A4E3B0D332D3BED324E3B0D338E3B041100000000D043314CD254345DF35C8F43A921A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A60886000A222711AC2A0709C9196811E7084B22A608A919A919C6080000FFFFFFFFFFFFFFFF6529E508E708C9196811E600C318DEF7FFFFFFFFFFFF0000E708A60888198911C608EA21EA1948110A228911A919",
    "0A22E708EA216811E608E92165002711A919C6080A22A6088500EA214500271168114500EA21A60065000A224500271168114500E921271145006811271145000A22850065002A224500E708C91945008919071145002A22850065002A224500C608EA214500271188194500C919071145000A22A60845004B2285004500A91948114500E919071145000A22C60845002A22A60845002B22A60065004B2A6B2A3154D25C2E3BAC32AF438C322A2A4E3B6B2AEA21CC32E92107114B2A2A22E71088198C2A2A2A4819",
    "0D3B2B2A6819C3181BDFFFFFFFFF3CE78D6B6529694A58C6FFFFFFFF3084A408C9218C326B2A27116B2A8C2A0A224819CC324B2AE92168190D3B2A22E92188190A220D33A919E9216B2A8E3B8B2A8B2A4B2A8F43AC2AC30895ADFFFFFFFF0000A919CC32AC2A6E3BAC2ACC320A222D3BCC324E3BAC2AED326B2A2E3BAC2A6E3BED326E430D334E3B0D3B6E3B4E3B8F43041195ADFFFFFFFF0000135DCF4BEA21A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608",
    "2A2207096B2AC608C919E921C6082A22E6086811EA2165000000FFFFFFFFFFFFFFFF30840409E708E708471125118D6BFFFFFFFFFFFFFFFF00002A228911C921C919C6086819C919A608C9214811E7082A22E708A9192A2207098B2A891127116B2AA600271168114500EA21A60045002A224500070989194500C919C60845002A224500C608AC32860045002A224500C608C92145008819271145000A22A60865002A224500C608EA214500481188194500A919071145000A22A60845002A22850086002A226500",
    "C6080A22850045000A22C60845002A22A60865002A22860085002A22850085002A22850085002A228600EA21B25C51542D3BCF4BCC324B2A6E3B8C32EA21ED3A4B2A07110A220D3B6819A919EC32AC322711CC32A30854A5FFFFFFFF38C66108A619A819061141002C634D6BA308AC326B2A68192A2AED32A92188198B2ACC326819C9218B2AAC324819C921AC328B2A6B2AE708CC320D332D3BA919ED32ED326E3B0A22AC32820818C6FFFFFFFF00004E3B0D330D3B4B2AEC328C2A6E3BEC320D3B8C2A0D3BAC2A",
    "6E3BED326E3B0D334E3BED326E3B4E3B8F43AF43CF43304C041118C6FFFFFFFF0000C608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A6084B22E7086B22E70848112A228600EA19481107090A2285000000FFFFFFFFFFFFBEF7FBDE2100C91948114819210058C6BEF7BEF7FFFFFFFF00008819E708E608C81163004200A30863002709EA19A6080A22E7084300A3082300C5086200000000000000000087194500E708891945000000000000002A22",
    "4500C608C92123000000000022008811C9214500881127114500EA21A60865000A226500E708C9214500481188194500A91907114500EA21C6088600EA21A608C608E92185000709C92165004711A91945006811E921C608A600EA21A608C608EA21A608C608EA21A608C608EA21A608C6080A22A608C6080A22C608A6000D335154515C104C2D3B6B326E3BCC3A0A222D3BED3A0A2A2A224E436B32C9216B2A0D3B26114529FFFFFFFF9DEFE3182A2A2711AC326B2A2A2AA408C721EA29A9210A220D3B88190511",
    "C308C308A40887198B2A8C3289190A228B2A4619830846198719C3080000000000008208E921CC322A222D3B66192000BAD6FFFFFFFF0000A30866198C2A2E3BCC32492A0511C308E4100822CC2A6E3B0D336E3B2D3B6E3B6D33492A45112411C721AE43335D892A2000BAD6FFFFFFFF000021004300A608A608840042002100220063008500A608A608A608A608A608A608A608A608A608A608A608A60886086B2A85002A228819E7080A22C6088811C921A6002A22E7080000FFFFFFFFFFFFFFFFFFFFE739C508",
    "4811A8192421FFFFFFFF7DEFFFFFFFFF00000A220A22830082102C6354A595ADEF7B2421E408E708A9192511EF7B54A5AA52C73917BEBEF7DEF7DEF7BAD62842630045002A220000FFFFFFFFFFFF0000271145002511CB5AFFFFFFFFEF7B2200C919C608A600E9218500E708A9194500681168194500891927118500A919E708C608A919A608070989198500271188196500681188196500881188196500881168198500A9198819A60827118819A60827118819A60827118819A60827118819C6082711A919E608",
    "0709A9190711C608A9194B2ACF4BD25CCF4BCC3A6E434E43EC3A2E3B4E43CC3AE921CC328B324A2A0A2266191084FFFFFFFFB294A3084B2ACC3AC9214A2A4B2ACC3A68198B2A4B2A8B32C4088210EB5A34A595AD5084863162088819CC320A2A2511EF7B54A5AA52C73917BEBEF7DEF7DEF7BAD628428719ED324619EF7BF7BDBEF7FFFFFFFFF7BDF7BDEF7B8619451141002842929496B534A5EB5A82106A2A6E3B8E3BAF43492A8208CA5214A595AD718CA6314100EF7BF7BDBEF7FFFFFFFFF7BDF7BDEF7B4200",
    "2100A631518C95AD54A52C63A2104200A608A608A608A608A608A608A608A608A608A608A608C6088B2AC6088811E921A600C919271107110A22A6086811E9210000FFFFFFFFFFFFFFFFFFFFB294A30807094300EF7BFFFFFFFF7DEFFFFFFFFF0000A60882084D6BBEF7FFFFFFFFFFFFFFFFFFFF718C410045000000FFFFFFFF1BDFFFFFFFFFFFFFFFFFFFFFFFFFDEF7A631240068110000FFFFFFFFFFFF0000E9216500A400CB5AFFFFFFFFEF7B220007118819450068116819650068112711A6086819E710E708",
    "6819A60847116819860048116819850048118819A60048116819C60848116819C60827116819E70827116819650068114819C60868114819C60868114811C60868114711E60848114711E708481147110711271127114819071148116819A919EF4B136D1054EF4B8E4B2D436E430D3BAF4BAC32EC322A2AAC32620817BEFFFFFFFF2842A7196B320A228C320A2A8B32EA21AC32E9218B32A3080C639DEFFFFFFFFFFFFFFFFFFFFFF7BDE3186719CC320000FFFFFFFF1BDFFFFFFFFFFFFFFFFFFFFFFFFFDEF7A631",
    "49220000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF41080F7C9DEFFFFFFFFFFFFFFFFFFFFF3CE786310C33104CA619EB5A9DEFFFFFFFFFFFFFFFFFFFFFB6B5FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF6108D6B5FFFFFFFFFFFFFFFFFFFFDEF7918C4300A608A608A608A608A608A608A608A608A608A60885002A2268114711E921A6086811C921C608C91907110711E9210000FFFFFFFFFFFFBAD6DEF75CE7410009222000BAD6BEF7DAD6DEF7FFFFFFFF00006300AE73FFFFFFFF59CE2842494ABAD6FFFFFFFFCF7B",
    "83080000FFFFFFFFFFFFDEF7B294042128429DEFFFFFFFFF34A52200C6080000FFFFFFFFFFFF0000681148194300CB5AFFFFFFFFEF7B8308860048112711E7082711E71027112711C60848114819A60847116819C60827116819C608070968190709E70868192711C60868194711C60848194811C60848194811C608A92107110711481907110711481907110711271127112711071147114811E71048116811E60848196811C608681968116819E9212A22EA21EF53D264715CAF4B6E4BAF43CC324D430D3B4E43",
    "0A2A21003CE7FFFFFFFFC3184A220A228B324B2AAC32A921CC3A4A2A8B2AC4106D6BFFFFFFFF9DEFD294918C5CE7FFFFFFFFDAD6A2102A220000FFFFFFFFFFFFDEF7B294042128429DEFFFFFFFFF34A504116619CF7BD6B5FFFFFFFFFFFFD6B5D6B5CF7BE739FFFFFFFF3CE7918C4D6BD6B5FFFFFFFFBAD6A208892A4C63FFFFFFFFDEF775AD30849AD6BEF73CE73084D6B5FFFFFFFFFFFFD6B5D6B5CF7BD294FFFFFFFF96B5A6318A5299CE1BDFB2944200A608A608A608A608A608A608A608A608A608A608C608",
    "A919C919E608A919471127110A22C608C6080A222711C6080000FFFFFFFFFFFFEF7BFFFFFFFF694AE508A631FFFFFFFF0F7CFFFFFFFFFFFF00004529DEF7DEF758C6410806112300C2109DEFDEF79DEF61080000FFFFFFFFFFFF08420100881122003084FFFFFFFF1BDF200045000000FFFFFFFFFFFF0000070948116300CB5AFFFFFFFFEF7BA408E71048112711C60848112711C60807096819E708E70868190711C60868194711A60868194811A60848194811A60868194811A60868194811A60868194811A608",
    "68194811450068196819A60848198819A60827118819C6080711A9190709E60889192711A6088919681185008819A919E710C9214B2A891988198B32A9198819EF53D264915CCF4B30548E432D430D43CF4B0000DEF7FFFFFFFF20004B2A0D3B2A220A2A4A2A2D3BA9194A2AE8210421BEF7DEF79DEF25216208E310C2101BDFFFFFFFFFB29465190000FFFFFFFFFFFF0842E3104922C3083084FFFFFFFF1BDF41000D3B86190000FFFFFFFFFFFF0000C308C721C721694A694A4100A619692A410058C6FFFFFFFF",
    "E7390421BEF7DEF7DEF7E7392100420021002421242142000000FFFFFFFFFFFF0000210000007DEFFFFFFFFF6108640064002100410863088608A608A608A608A608A608A608A608A608A608A60865004811C921C6082711E921E70827116811E70888190A2265000000FFFFFFFFFFFF2421FFFFFFFF34A54100718CFFFFFFFF4529FFFFFFFFFFFF0000B294FFFFFFFF2842E508CC322711620054A5FFFFFFFF494A0000FFFFFFFFFFFF00006500481105118A52FFFFFFFFDEF70000E7080000FFFFFFFFFFFF0000",
    "A6002711E508CB5AFFFFFFFFEF7B63004819E708E70868192711A60868194811860068194811850068194811860068194711A60868190711A60888190709C6088919E708C608A919E708C608A919E608A608C921681165004819C91965002711C91985000711E921A600E608EA21C608A608E92127118500A919C91985004A2A6B2A0711E9216B2AA91927116B2A4B2AA6084A2A8E43915C9164B164104C4E43EC3A0000DEF7FFFFFFFF00008B328919CC3ACC324A2A88190D3BAC32E410518CFFFFFFFFEF7B8308",
    "EC3AAC328719EB5AFFFFFFFFBEF782100000FFFFFFFFFFFF00006B2A0D3B28228A52FFFFFFFFDEF700002D3B8E430000FFFFFFFFFFFF0000AF43CF43CF43CB322C3B135DD15C0B3BA2081084FFFFFFFF6D6B5084FFFFFFFF918C2100A608A608A60885008500A6080000FFFFFFFFFFFF0000A60800007DEFFFFFFFFFC73922008500A60885088608A608A608A608A608A608A608A608A608A6088608A608C608E7086811471127110A22E708C6080A226811C608A919A9190000FFFFFFFFFFFF000058C6DEF79DEF",
    "61081BDFBEF779CE0000FFFFFFFFFFFF0000BAD6DEF7DEF7200000000000000000003084FFFFFFFF8E730000FFFFFFFFFFFF00002711C608E5102842FFFFFFFFFFFF000048190000FFFFFFFFFFFF0000681927116300CB5AFFFFFFFFEF7BC408A60068194811850068194811850068192711A6088919E708C608A919C608C608A919A608E608C919A600E710E9218500E710EA218500E6080A228500C6080A228500A6080A22891945006819C919450027110A224500E6082A228500A6082A2AC60865000A224811",
    "27110A2AAC32E70888190D3B07116819AB32E9212711C919EC32C60888198B322D3BED3AB264D264105421005CE7FFFFFFFFC3186B2A4D43E9212A2AAC324E4389192A2A610899CEFFFFFFFF6529AA324B2ACC32CB2A8208DEF7FFFFFFFF694A0000FFFFFFFFFFFF00008E432D3B4A2A2842FFFFFFFFFFFF00006E436E3B0000FFFFFFFFFFFF00005154D25C336513658A2A4100A210CB5AD39CDBDEFFFFFFFFEF7B79CEFFFFFFFFA6318400A608A608A608A608A608A6080000FFFFFFFFFFFF0000A6084200B294",
    "FFFFFFFFBEF73084A2102100840086088608A608A608A608A608A608A608A608A608A608A6088500E60807090A220709E708E921E70827112A22A608881927110000FFFFFFFFFFFF00008D6BFFFFFFFFCE73FFFFFFFFAE730000FFFFFFFFFFFF0000BEF7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEF7B0000FFFFFFFFFFFF000048198600E5082842FFFFFFFFFFFF0000C6080000FFFFFFFFFFFF0000E6082711A400CB5AFFFFFFFFEF7B83082711A6088819E708C608A919C608E608A9198600E710C919",
    "65000711C91945000711C91945000711E91945002711E92145000711EA214500E7100A224500C6082A22650045008919881945006819C919450007110A224500A6084B2AA60065002A228919E6088B322A220711C921AC326819E710EC32C921C6084B2A4B2A68190711EC32C921E6084819EC3AE9218B32715C241918C6FFFFFFFF084249324B2A4D43CC326B3289192D3BAC3200009DEFFFFFFFFF410829226E3B4E3BAF4341001BDFFFFFFFFF8E730000FFFFFFFFFFFF00002D3B6E43292A2842FFFFFFFFFFFF",
    "0000AF43CF430000FFFFFFFFFFFF0000135DEF432A2241084D6BFBDEFFFFFFFFFFFFDEF7FFFFFFFFEF7B9DEFFFFFFFFF41088508A608A608A608A6088608A6080000FFFFFFFFFFFF0000A60885084108139DFFFFFFFFFFFFDEF7F7BDC6314200A608A608A608A608A608A608A608A6088608A6088608A60868190711A9194711C608C921C919A60888198819A91988190000FFFFFFFFFFFF0000C318DEF7DEF7BAD6FFFFFFFFE3180000FFFFFFFFFFFF0000BEF7FFFFFFFF79CE79CE79CE79CE79CE79CE79CE75AD",
    "28420000FFFFFFFFFFFF000027116819A5002842FFFFFFFFFFFF000047110000FFFFFFFFFFFF0000A6084819E508CB5AFFFFFFFFEF7BA408A91986000711C91965000711C91945004711A91945004819891945006819881945006819881945006819881945006819A91945004711C91945000711EA214500C6082A2285004500A919881945006811E9214500E6084A2A850085006B2A881907118B324B2AE608EA21CC3207118819AC32C92107110A22AC32E608A9198B32E9210711EA21AC328819E708EA218B32",
    "A919E4105084FFFFFFFF718C8619F053EC3AEC3ACC324D432A2A6B320000DEF7FFFFFFFF00008E434D3B4D430D336100FBDEFFFFFFFFCF7B0000FFFFFFFFFFFF00006E3B6E438A2A2842FFFFFFFFFFFF00007154D25C0000FFFFFFFFFFFF0000A6088500A31079CEFFFFFFFF1BDFCF7B04216D6BFFFFFFFFEF7BDEF7FFFFFFFF00008608A608A608A608A608A608A6080000FFFFFFFFFFFF0000A608A60885082100852955ADBEF7FFFFFFFFFFFF0F7C2200A6088608A608A608A608A608A608A608A608A608A608",
    "0A22E708C6080A2227110711E921481168192711C6088B2A0000FFFFFFFFFFFF00006200D7BDDEF7DEF7BEF717BE01000000FFFFFFFFFFFF0000DBDEFFFFFFFF8210610041004100610021004100830864000000FFFFFFFFFFFF00004811E710A5082842FFFFFFFFFFFF000048110000DEF7FFFFFFFF410847094500E508EB5AFFFFFFFFEF7B2200681988194500881968114500A91948114500C92127114500C92107114500E92127114500E92127114500C91948114500A919681945006819C919450027110A22",
    "4500C6088B2A47114500A919A919450027110A226500C6088B3227112711CC320A2A27110A228B3288194811CC32E921E7108B2A2A2A88196819AC32E92107114A2A4A2AC92107118B2A6B2AEA2127114B2AA821A631FFFFFFFF7DEFA2104D43EF4B8E4B8E4BCC3AC821041120003CE7FFFFFFFFE318EC324D3B6E434E3B2000BEF7FFFFFFFF2C630000FFFFFFFFFFFF0000CF43CF43CB322842FFFFFFFFFFFF0000F25CCF430000FFFFFFFFFFFF0000A608220034A5FFFFFFFFB2942000430042006D6BFFFFFFFF",
    "EF7B5CE7FFFFFFFF03198500A608A608A608A608A608A6080000FFFFFFFFFFFF00008608A608A608A608850021008210718CFFFFFFFFFFFFC7396400A608A608A608A608A608A608A60886088608850068198819E70888192A22860088196811A919E921450068190000FFFFFFFFFFFF000025110C63FFFFFFFFFFFF4C63A4080000FFFFFFFFFFFF0000F39CFFFFFFFFAA52E5088819850068116819C608271107110000FFFFFFFFFFFF00000711E708E5102842FFFFFFFFFFFF0000881920001BDFFFFFFFFFC739",
    "26118400A210DAD6FFFFFFFFEF7B43004500E921070945000A22E60845002A22C60845002A2AC60845002A2AA60845002A22C60845000A22E6084500EA2107114500C919681945008819A921450027110A2265004500C919681945006811E921650027112A2AC9214819AB322A2A07114A2A8B322711C9198B32C92168192A228B324711C9216B2AE9218819E9218B326819A9190A226B326819C921E9218B328819E9216200B6B5FFFFFFFFB6B561080B3B1054EC3AA2080B5BF39C0C63B6B5FFFFFFFFEB5A0822",
    "AE436E3B8A32073AFFFFFFFFFFFFA6310000FFFFFFFFFFFF00005054B25CAE432842FFFFFFFFFFFF0000E608C6080000DEF7FFFFFFFF0000C60800007DEFFFFFFFFF82108500A60843006D6BFFFFFFFFEF7BD7BDFFFFFFFF2C634300A608A608A60885088508A6080000DEF7FFFFFFFF0000A608A60864006400A608A6088508000034A5FFFFFFFFAE734300A608A608860886088608A608A6088608A608C608E7080A228819A608A919C9194819681965004B2AC919A6080000FFFFFFFFFFFF00004A228210BEF7",
    "DEF7DEF7A21088190000FFFFFFFFFFFF00008631FFFFFFFFBAD6610846118811471182082100A40848190000FFFFFFFFFFFF00006819481163002842FFFFFFFFFFFF0000A608820875ADFFFFFFFF58C624212842FBDEBEF7FFFFFFFFEF7B2511A60865004A2A860065004A2A850065004B2A850065004A2A850065004B2A860065004A2AA60845002A22E60845000A2227114500C919681945006811C921650027110A22E60845008919A91985004811CC32A9218819AB320A2A68194B2A4B2A8819A9198B320A2A",
    "47116B2A2A2AA921C9214A2A2A2A68194A2A0A220A2A88194B2A0A2AE921EA214A2AEA214A2AC9214A2AEA21E82104215CE7FFFFFFFFDAD68A528210A63195ADFFFFFFFF5CE72C63FFFFFFFFBAD66108AA32EB324100F7BDFFFFFFFFDAD681080000FFFFFFFFFFFF00003365B1548A2A2842FFFFFFFFFFFF0000C608C60800005CE7FFFFFFFFA210640000009DEFFFFFFFFF821085006400210075ADFFFFFFFFEF7B4D6BFFFFFFFF1BDF821064008500430082108210640000005CE7FFFFFFFFA21064004200C739",
    "C73921006400850021009294FFFFFFFF4C636300A608A608A608A608A608A608A608A608A6086500271107112A2207096819681147114A2AC608A6080A22E7080000FFFFFFFFFFFF0000A60846110C63F39C2C6383086B2A0000FFFFFFFFFFFF0000A3081084FFFFFFFFDAD6694A6108C73975AD38C68E6B63000000FFFFFFFFFFFF0000A6088819A5082842FFFFFFFFFFFF00008500C508E739DEF7FFFFFFFFFFFFFFFFFFFFBAD6FFFFFFFFEF7B63004A2A4500A6084A2A4500A6084A2A4500A6084A2A4500A608",
    "4A2A650086004A2A850085002A22A60865002A2207116500E92148194500A919A91965004811E921A6080709E921881985000A226B32C921C9196B2A2A2A68194A2A6B328819E9214B2AEA21C9210A224A2AC921C9214A2AE921EA21C9194A2AE921EA21EA210A222A2AE9210A22E9214A2A2A2A2A22E9212A2A2A2A6B2A261185293CE7FFFFFFFFFFFFFFFFFFFFFFFFFFFF7DEFE739820879CEFFFFFFFF58C6A6314529D6B5FFFFFFFFFFFF894A8D430000FFFFFFFFFFFF00006819E710A5082842FFFFFFFFFFFF",
    "0000C608C608210075ADFFFFFFFF718C6529452995ADFFFFFFFF54A5E318494ABAD6DEF7FFFFFFFFEF7B4108FBDEFFFFFFFFDAD62842C318AE739DEF5CE78629210075ADFFFFFFFF718C65294D6BFFFFFFFF38C648428210CB5ABEF7DEF79DEFC3188500A608A608A608A60886088608860886088608A608A919A60868190A222711A919A60868196B2AE608C921E6080000FFFFFFFFFFFF0000A608E9218408010026118500E7080000FFFFFFFFFFFF00002A2262088E73BEF7FFFFFFFFFFFFFFFFFFFFDEF7518C",
    "83000000FFFFFFFFFFFF0000E608071126112842FFFFFFFFFFFF0000A9194500C408894AFBDEDEF7DEF79DEF718CA2101084F39C083A2300E6080A224500E6080A224500E7080A224500E6082A2A6500E6082A228500C6082A22A608A6000A22E6088500E92147116500C919881965008819A921A6082711A91927116819E9216B32E921E9216B2A2A2AA9194A2A4A2AE9210A220A222A2AC9210A222A2AE9210A22E9212A2AE9210A22EA210A2A2A2AE9210A2AEA216B2AA9190A2A0A2A6B2AC9190A22C9218B32",
    "2A222A22A9216B32C8198210EF7B1BDFDEF7DEF7DEF75DEFB294C3182C3B905C2421FBDEFFFFFFFFFFFFFFFFFFFFFFFFFFFFCF7B86198E430000FFFFFFFFFFFF0000E710E710A5082842FFFFFFFFFFFF0000C608C608850086319DEFFFFFFFFFFFFFFFFF8A521BDFFFFFFFFFFFFFFFFFFFFFF7BDFFFFFFFFEF7B420065295CE7FFFFFFFFFFFFFFFFFFFFFFFFBEF72842630086319DEFFFFFFFFFFFFFFFFFB6B5DEF7FFFFFFFFFFFFFFFFFFFF7DEF28426400A608A608A608A608A608A6088608A608A608A608A608",
    "E921E919C608681927110A22A9194500E92107114711CC32C6080000000000000A2227110711CC322711C9194A2AC608E921000000000000071107118819430082100B5B34A5B6B514A5EB5A8210630047110711000000000000271188196500681905110000000000004500C92127114500671120000000000000000200E92143000100A719071145004711E92145002711E921450027110A22450007110A228500E7080A22A608C608EA21E608A600E92127118500A919881986008819A919A6084711A919C921",
    "88198B326B320A2AEA214A2A2A2AE9210A224A2A0A220A222A2A2A2A0A220A220A2A4A2A0A22E9210A2A2A2A2A22C9214A2A2A2A0A22A9194B2A4A2AEA21C9212A2A8B2A0A22E921C9214B2A8B322A22EA210A2A8B324D3B2D3B2D33A7196100000000000000410085196E3BEF4B0F4CEF4BC31030845CE7DEF7DEF7BEF738C6E739A30827112711071100000000000007110711E710E710A508000000000000E608E710E608E608A400E318AE7375AD34A5EB5AC4106108EB5A34A575ADCE7304214108E7398A52",
    "251985006400C318D2947DEFDEF7DEF7BEF775AD24216300A6086400E318AE7375AD34A5EB5A82108210EB5A14A5B6B514A5CA5241086400A608A608A608A608A608A608A608A608A608A608A6086500E710C919A9196819681185006B2AA91907110A2245006819C919E708CC324711E7084B2A4500071189194500C9194819E7082A224811A9192A2248110A220A2267110509620041006200A4080609070948196819A608481988196500881988194500A92147114500E921070945002A2AC60845002A2AA608",
    "65004A2AA60065004A2AA60085002A22A91945006811C92145004811E92145004711E92165000711E921A608E708E921E608C608C9212711A608A9196819A60888198819C6086811EA2188190A224B2A4B32EA212A222A2A4B2A0A220A222A2A2A2A2A22E9212A2A4A2AE9210A224A2A4A2AC9210A2A6B320A22C9210A2A8B320A22C9212A2A6B324A2AC9210A2A8B328B2AE921E9216B2AAC326B2A2A2AEA21CC322D3BCF438E438E43AF43CF43CF43CF43CF4B0F4C30547154D15CF25CF25C125D914C07224100",
    "0000000000008208A719891948194819271127112711271107110711071107110711E710E710E710E710E710E710E608E608C5006300410042006300A508A5086300420021006300A500A608840084008500C608C608A5004200000000000000000021008500A608A608A6088500430021002200630085088500630022002100220064008508A608A608A608A608A608A608A608A608A608A608A608A608C60807116819881968114A2A0709A608E921C6082A220A22A6082A22A60085004A2AA608C9194A2A2711",
    "8B2AA91927114A2A07114711C921A60068118819A60847116819A608471127110711071148196811A608681988198500881968116500C921471165000A22E70865002A2AC60865004A2AA60085004A2A860085004A2A860085004A2AA60045008819A91945006811C92165004711E92185002711E921A608E708E921E710C608C9192711C60888198819C60868198819E7106819A9214B2A2A2A4A2A4B320A2A2A224A2A4B322A2A0A220A2A4B2A2A22E9212A2A6B2A0A22E9214B2A6B2AC921E9216B326B2AC921",
    "E9218B324B2AE921C921AB328B2A0A22E9216B32AB324A2A0A222A2A8B328B324B2A4B2AAC32AE43EF4BEF4BCF43CF4BEF4BEF4B50547054B15CF25CF25CF25CF25CB154504C4D3B2A22070945004500450068196E43715CEF530D3B6B32C9216819481948192711271127110711071107110711071107110711E710E710E710E710E710E608E608E608E608E608C608E608C608C608C608C608C608C608C608C608C608C608C608C608C608A608C608A608A608A608A608A608A608A608A608A608A608A608A608",
    "A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A60885002A220711891907110711AB2A27118819681945004A2A47114811EC32E7088819E9214500C919E71085002A22E60868112A2A27110A222A2A88192A22C92107110711A9210711271168194811C608881988198600891968118500C921471165000A22E70865002A2AC60885004A2AA60885004A2AA60085004A2AA60085004A2AA60885004A2A271145006811E92165004711EA2185002711E921A6080709E921",
    "0711E608A9196819C60888198819C60868198819271188196B328B324A2A4A2A6B324A2A2A2A2A2A6B324A2A0A220A2A6B324B2AE9212A2A8B324A2AE9214A2A8B320A22C9216B32AC320A22E9216B32AC322A2AC9216B2AAC326B2AE9212A2A8B32AC324B2A4A2A8B32AC32AB32EC3AAE43EF4B104C304C304C30547154B15CF25CF264F25CD15CD154B154304C4D3B0A22E70845004500450045004500450045004500450085000A22EF53515CAF4BCC3A0A2A8819681948194819271127112711071107110711",
    "071107110711E710E710E710E710E710E710E608E608E608E608E608E608C608C608C608C608C608C608C608C608C608C608C608C608C608C608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608C6088819C91988190A22A6082711E921E708AB2A8819A6084A2A450007110A22A6086B2AEA214811CC32481168114A2AC60888198919860068196819E608271148194711E60868194811E60868198819A608",
    "891988198500A921481165000A22071165002A2AE60885004A2AC60885004A2AA60885004A2AA60885004A2AC60885004A2AE6088500E921EA21650047110A22A6080711EA21E608E708E9212711E608C9196819C60888198819E7106811681988194A2A6B328B324B2A4B2A6B326B324A2A2A2A6B326B2A0A222A2A8B328B320A220A2A8B326B2AE9212A2AAC326B2AE9212A2AAC326B2AEA210A2AAC328B320A2A0A2AAC32AC324B2A2A2A8B32CC32AC328B328B32CC320D3BCF43EF4B70547154B15CD25CF264",
    "F264F25CB15C9154B1549154304C2D330A22E6084500450045004500450045004500450045004500450045004500450045004500E608CC32505C505C4E438B320A2A8819681948192711271127110711071107110711071107110711E710E710E710E710E710E710E608E608E608E608E608C608C608C608C608C608C608C608C608C608C608C608C608C608C608A608C608A608A608C608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A6088608",
    "C608A919E7088819AB2AE708A919471145006B2A27118819CC32C608C91988194500E921C608C6082A22E608A9192A2A27112A222A2AA919A91968192711071148194811071168196819C60888198819A608A92188198500E921481165002A2A271165002A2AE70865004A2AE60885004A2AE60885004A2AE71065002A22271165000A224819450007110A22C608E7080A222711C608E9216819C608A9198819E7088819891927114811C9218B324B2A8B328B328B324B2A6B328B328B2A2A2A4B328B324A2A0A2A",
    "8B328B322A2A0A2A6B32AC324A2AEA218B32CC322A2AEA218B32CC324A2A0A2A6B32CC328B324A2A6B32CC32CC328B328B32CC32EC3AEC3AEC3A0D3BCF4B5054F25C12651265F264D25CB15470547054915491542F4C0C33C919A6084500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450068194E43505C505C8E4B6B32C9216819681948192711271127112711071107110711071107110711E710E710E710E710E710E710E608E608E608C608C608",
    "C608E608C608C608C608C608C608C608C608C608C608C608A608C608C608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608C6082A226819E921A60048112A222711AB328819A6084A2A850007112A22C6086B2A0A224811AB32481148112A2AA60868118919A6086819681907114711681947110711A919681907116819A919C6088819A9198600A91988196500C92188196500E921681145000A22481145000A22481145000A2268194500",
    "E92188196500C919A921850088192A2A2711C608E9216819C608C919A919E6088819A91927114811A9192711A9198B328B32AB328B2A8B328B328B2A4B2A8B32AB324B2A4B2AAC328B322A2A4B2AAC326B2A2A2A4A2AAC32AB322A2A2A2ACC32AC324A2A4A2ACC32CC326B2A6B2AAC32CC32AC328B32CC32EC3AEC3AEC3A0D3B4D438E4BEF4B5054B15C12651265D15C7054504C704CB154B1549154EF43AC2A88198500450045004500450045004500450045004500450045004500450045004500450045004500",
    "4500450045004500450045004500450045004500450045000711CC32505C305C4D432A2A8919681968194819271127112711271107110711071107110711E710E710E710E710E710E710E710E710E608E608E608E608C608C608E608C608C608C608C608C608C608C608C608C608C608A608C608A608C608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A6088608C92188198819AB32C6086819A91945006B2AA9196819CC32E7086819E9214500C9194819A6082A22",
    "681988194A2A88192A224A2AE708681968194711271168196819E7082711881907112711C921C6084811E92186006819E92165008819E9214500A919C9194500A919E9214500A919E921450088190A22850068110A22A60827112A2AE608A608A919C921C6088819C92107114711C919481927118819E9216B2A8B2ACC3ACC32AB328B32AC32AC326B2A6B32CC328B324B2A8B32CC326B2A4A2AAC32CC326B2A4A2AAC32CC32AB324B2A8B32CC32AC328B328B32CC32CC32AC32CC32EC3A0D3B0D3B0D3B4D438E4B",
    "EF533054505C715C3054305470547054504C504CB154F154D154504C0C33E91907094500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450068192D43515C1054ED3AE92188196819681948192711271127112711071107110711071107110711E710E710E710E710E710E608E608E608E608E608E608C608C608C608C608C608C608C608C608C608C608C608C608C608C608",
    "C608C608A608C608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608A608E608E9212711E708AC3268112A220A2245000A220711C6088B2A27110922AB3227114A2AE921E6080A2227110711A919271147118819471168116819881927116819A9196811E710C9212711E710E9210709E7102A22C60807112A2AA60807112A2AA60807114A2AA60807112A2AC608E7082A2AE608E6082A222711C6080A226819C608E921E92107112711E92168192711C91988194711E921CC328B32EC32",
    "CC32CC3ACC32AB328B32CC3AAC328B328B32CC328B326B2AAC32CC328B326B32CC32CC328B326B32CC3AEC3ACC328B32CC3AEC3AEC3ACC3AEC3A0D3B2D3B2D434E43AF4B0F54505C715C715C3054CF4B0D3B8B2A8B2A6E3B704CD154125D125DF1542F4CCC3288196500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500A9196E43",
    "715CEF53AC32A92188196819681948192711271127110711071107110711071107110711E710E710E710E710E710E710E608E710E608E608E608C608C608C608C608C608C608C608C608C608C608C608C608C608A608C608A608A608C608A608C608A608A608A608A608A608A608A608A608A608A608A6088B2A48116B2A4811A6084A2AC608C919AB3227118B2AA91985002A22E71007110A22E710E9212A2288196B2A2A22C9198819681968194819881968192711A9194711E6088919C919C608C921A819A608",
    "0A22881986000A22681985002A2A681985002A22881985000A22881985000922A9218600C919E921C6088819E921E7106819E921471127118819C92127116819C9210A2A8B32EC3A0D3B0C3BEC3ACC3AEC3AEC3AAC32AC320C3BCC328B32CC3AEC3AAC328B32EC3AEC3AAC32AC32EC3AEC3AEC3ACC3A0D3B2D3B0D3B0D3B2D3B4D436E43AE4BEF533054715C915C715C3054AE4B0D3B8B2A0A22A819881909228E3BD15453657365F25CEF438B2A4711450045004500450045004500450045004500450045004500",
    "45004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045004500450045006500E921AF4B715CCF4B4B2A881968196819681948194819271127112711071107110711071107110711E710E710E710E710E710E710E710E710E608E608E608E608C608C608C608C608C608C608C608C608C608C608C608C608C608A608C608A608A608A608A608A608A608A608A608A608A608A608A608"
    };
    // Create a QByteArray
    QFile file("output.bin");
    if (!file.open(QIODevice::WriteOnly)) {
        qFatal("Failed to open file");
    }
    // Create a QDataStream
    QDataStream out(&file);

    for (uint i=0;i<sizeof(lcdbufstr)/sizeof(lcdbufstr[0]);i++){
        QByteArray ba = QByteArray::fromHex(lcdbufstr[i]);
        // Open a file for writing

        // Write the QByteArray to the file
        out << ba;
    }

    // Close the file
    file.close();

}

void TerminalView::on_recInfo_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("recInfo\r\n");
    sendDataRequestEx(array);
}

