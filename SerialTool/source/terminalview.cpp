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
#include <QTimeZone>
#include <QRandomGenerator>
#include <QThread>

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
QString curPortName;
QString TerminalView::getCmdHead(){

    if (curPortName.contains("Quectel", Qt::CaseInsensitive)) {
        return ">";
    }
    if (ui->kHead->isChecked()){
        return ">";
    }
    return "AT##INRICO>";
}

QString TerminalView::getKeyEventType(){
    if (ui->keyEvent->currentText() == "press"){
        return ",1\r\n";
    } else if (ui->keyEvent->currentText() == "release"){
        return ",0\r\n";
    }
    return ",\r\n";
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
    if (curPortName.contains("Quectel", Qt::CaseInsensitive)) {
        array.append(getCmdHead()+"103\r\n");
    } else{
        array.append(getCmdHead()+"reset\r\n");
    }
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

    array_text =code->fromUnicode(ui->APN->currentText().toUtf8());
    if (0==array_text.length()) {
        array.append(array_text+",");
    } else {
        array.append(",");
    }

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
        while (ui->historyBox->count() >= 200) {
            ui->historyBox->removeItem(199);
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

void TerminalView::setPortName(const QString &pname)
{
    curPortName = pname;
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
    } else if (cmds == "autoCall5590"){
        QString cmdout = getCmdHead()+"autoCall,5000,5000,90000,";

        QTextCodec *code = QTextCodec::codecForName(m_codecName);
        QByteArray array_text =code->fromUnicode(ui->apnPwd->text());
        if (0==array_text.length()) {
            array.append(cmdout);
        } else {
            int totalTime = 100000;
            int sendTime = array_text.toInt();
            if (sendTime*2>=totalTime){
                QMessageBox Msgbox;
                Msgbox.setText("time err.");
                Msgbox.exec();
                return;
            }
            cmdout.sprintf("autoCall,%d,%d,%d,", sendTime,sendTime,totalTime-sendTime*2);
            array.append(getCmdHead()+cmdout);
        }
        array_text =code->fromUnicode(ui->APN->currentText().toUtf8());
        if (0==array_text.length()) {
            QMessageBox Msgbox;
            Msgbox.setText("Please enter the group ID in the APN textbox to send calls.");
            Msgbox.exec();
            return;
        } else {
            array.append(array_text+",");
        }

        array_text =code->fromUnicode(ui->apnUser->text());
        if (0==array_text.length()) {
            QMessageBox Msgbox;
            Msgbox.setText("Please enter the group ID in the apnUser textbox to receive calls.");
            Msgbox.exec();
            return;
        } else {
            array.append(array_text+",");
        }
        array.append("\r\n");
    } else if(cmds == "dmrAutoCall"){
        QString cmdout = getCmdHead()+"dmrAutoCall,";

        array.append(cmdout);

        QTextCodec *code = QTextCodec::codecForName(m_codecName);

        QByteArray array_text =code->fromUnicode(ui->APN->currentText().toUtf8());
        if (0==array_text.length()) {
            QMessageBox Msgbox;
            Msgbox.setText("Please enter the duration of the outgoing call in the APN text box.");
            Msgbox.exec();
            return;
        } else {
            array.append(array_text+",");
        }

        array_text =code->fromUnicode(ui->apnUser->text());
        if (0==array_text.length()) {
            QMessageBox Msgbox;
            Msgbox.setText("Please enter the duration of the incoming call in the APN user text box.");
            Msgbox.exec();
            return;
        } else {
            array.append(array_text+",");
        }

        array_text =code->fromUnicode(ui->apnPwd->text());
        if (0==array_text.length()) {

        } else {
            array.append(array_text+",");
        }

        array.append("\r\n");
    } else if (cmds == "syncDate"){
        auto now = std::chrono::system_clock::now();
        auto utc_time = std::chrono::system_clock::to_time_t(now);

        QTimeZone tz = QTimeZone::systemTimeZone();

        int tz15Min = tz.offsetFromUtc(QDateTime::currentDateTime()) / 60 / 15;


        array.append(getCmdHead()+"setdate,"+QString::number(utc_time)+"," + QString::number(tz15Min) + ",\r\n");
    } else if (cmds == "dmrOwnId"){
        QTextCodec *code = QTextCodec::codecForName(m_codecName);
        QByteArray array_text =code->fromUnicode(ui->apnUser->text().toUtf8());
        if (0==array_text.length()) {
            QMessageBox Msgbox;
            Msgbox.setText("Please enter the own id in the APN user text box.");
            Msgbox.exec();
            return;
        } else {
            bool ok;

            // Convert to int
            int value = array_text.toInt(&ok); // base 10 by default

            if (ok && value>0 && value<16777215) {
                array.append(getCmdHead()+"dmrat,AT+DMRRadioId=" + array_text+",\r\n");
            } else {
                QMessageBox Msgbox;
                Msgbox.setText("The ID must consist of digits only(1-16777214)");
                Msgbox.exec();
                return;
            }
        }
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
    array.append("keySimu,s"+getKeyEventType());
    sendDataRequestEx(array);

}

void TerminalView::on_keyUp_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,u"+getKeyEventType());
    sendDataRequestEx(array);
}


void TerminalView::on_keyCancel_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,c"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_keyLeft_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,L"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_keyOk_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,o"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_keyRight_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,r"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_keyCall_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,g"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_keyDown_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,d"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_keyRed_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,e"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_key1_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,1"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_key2_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,2"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_key3_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,3"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_key5_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,5"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_key4_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,4"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_key6_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,6"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_key7_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,7"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_key8_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,8"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_key9_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,9"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_keyStar_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,*"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_key0_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,0"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_keyHash_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,#"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_keyPtt_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,p"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_keySos_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,a"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_keyHome_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,h"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_keyF1_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,x"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_keyF2_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,y"+getKeyEventType());
    sendDataRequestEx(array);
}

void TerminalView::on_keyF3_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("keySimu,z,"+getKeyEventType());
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
    const char *lcdbufstr[] ={};
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


void TerminalView::on_setIP_clicked()
{
    QByteArray array;

    array.append(getCmdHead()+"101&");

    QByteArray array_text;

    QTextCodec *code = QTextCodec::codecForName(m_codecName);

    array_text =code->fromUnicode(ui->APN->currentText().toUtf8());
    if (0!=array_text.length()) {
        array.append(array_text);
    }

     array.append("&&4G&en&lcn&410.304&1&1&1&");

    array_text =code->fromUnicode(ui->apnUser->text());
    if (0==array_text.length()) {

    } else {
        array.append(array_text);
    }

    array.append("&T529&1");


//    QRegExp rx2 ("^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
//    QRegExp rx3 ("^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])$");
    array.append("\r\n");
    sendDataRequestEx(array);
}


void TerminalView::on_GrpInfo_clicked()
{
    QByteArray array;
    array.append(getCmdHead());
    array.append("grpInfo\r\n");
    sendDataRequestEx(array);
}


void TerminalView::on_logon_clicked()
{
    QByteArray array;

    int index = ui->logList->currentIndex();

    if (ui->logList->currentText() == "drvLog"){
        array.append(getCmdHead()+"drvLog,1,\r\n");
    } else if (ui->logList->currentText() == "txtLog"){
        array.append(getCmdHead()+"uploadLog,1,1,\r\n");
    } else if (ui->logList->currentText() == "dmrFreq"){
        array.append(getCmdHead()+"dmrat,AT+DMRSetFreqEdit=1,\r\n");
    } else if (ui->logList->currentText() == "assertMode"){
        array.append(getCmdHead()+"assertMode*I0,1,\r\n");
    } else {
        array.append(getCmdHead()+"Lvo"+QString::number(index)+ "\r\n");
    }

    sendDataRequestEx(array);
}


void TerminalView::on_logOff_clicked()
{
    QByteArray array;

    int index = ui->logList->currentIndex();

    if (ui->logList->currentText() == "drvLog"){
        array.append(getCmdHead()+"drvLog,0,\r\n");
    } else if (ui->logList->currentText() == "txtLog"){
        array.append(getCmdHead()+"uploadLog,0,\r\n");
    } else if (ui->logList->currentText() == "dmrFreq"){
        array.append(getCmdHead()+"dmrat,AT+DMRSetFreqEdit=0,\r\n");
    } else if (ui->logList->currentText() == "assertMode"){
        array.append(getCmdHead()+"assertMode*I0,0,\r\n");
    } else {
        array.append(getCmdHead()+"Lvc"+QString::number(index)+ "\r\n");
    }

    sendDataRequestEx(array);
}


void TerminalView::on_lcdClear_clicked()
{
      memset(lcdbuf, 0x12, 240*320*2);
      displayLcdScreen();
}


void TerminalView::on_memGetSet_clicked()
{
    QByteArray array;
    int varType = 4;
    if (ui->memType->currentText() == "uint32"){
        varType = 4;
    } else if (ui->memType->currentText() == "uint16"){
        varType = 2;
    } else if (ui->memType->currentText() == "uint8"){
        varType = 1;
    } else if (ui->memType->currentText() == "float"){
        varType = 3;
    } else if (ui->memType->currentText() == "double"){
        varType = 5;
    }

    QByteArray array_text;
    QByteArray array_text2;
    QTextCodec *code = QTextCodec::codecForName(m_codecName);

    array_text =code->fromUnicode(ui->apnUser->text());
    if (0==array_text.length()) {
        QMessageBox Msgbox;
        Msgbox.setText("apnUser输入十六进制地址：xx");
        Msgbox.exec();
        return;
    }
    array.append(getCmdHead());
    array_text2 =code->fromUnicode(ui->apnPwd->text());
    if (0==array_text2.length()) {
        array.append("memGet," + array_text+ "," + QString::number(varType) + "\r\n");
    } else {
        array.append("memSet," + array_text+ "," + QString::number(varType) + "," + array_text2 + "\r\n");
    }

    sendDataRequestEx(array);
}


void TerminalView::on_tmpIp_clicked()
{
    QByteArray array;

    array.append(getCmdHead()+"t1&");

    QByteArray array_text;


    QTextCodec *code = QTextCodec::codecForName(m_codecName);

    array_text =code->fromUnicode(ui->apnUser->text());
    if (0==array_text.length()) {
        QMessageBox Msgbox;
        Msgbox.setText("no imei");
        Msgbox.exec();
        return;
    } else {
        array.append(array_text);
    }

     array.append("&123&");

    array_text =code->fromUnicode(ui->APN->currentText().toUtf8());
    if (0==array_text.length()) {
        QMessageBox Msgbox;
        Msgbox.setText("no ip");
        Msgbox.exec();
        return;
    } else {
        array.append(array_text);
    }

    array.append("\r\n");
    sendDataRequestEx(array);
}


void TerminalView::on_setLang_clicked()
{
    QByteArray array;

    array.append(getCmdHead()+"setlang,");

    QByteArray array_text;


    QTextCodec *code = QTextCodec::codecForName(m_codecName);

    array_text =code->fromUnicode(ui->APN->currentText().toUtf8());
    if (0==array_text.length()) {
        QMessageBox Msgbox;
        Msgbox.setText("input lang code in APN textbox");
        Msgbox.exec();
        return;
    } else {
        array.append(array_text);
    }

    array.append(",\r\n");
    sendDataRequestEx(array);
}


void TerminalView::on_pushButton_clicked()
{
     QByteArray array;
    array.append("AT+QDOWNLOAD=1\r\n");
    sendDataRequestEx(array);
}


void TerminalView::on_pushButton_3_clicked()
{
    QByteArray array;
   array.append("AT+POCLOG=2,0\r\n");
   sendDataRequestEx(array);
}


void TerminalView::on_reset_clicked()
{
   QByteArray array;
   array.append(">103\r\n");
   sendDataRequestEx(array);
}


void TerminalView::on_resetBtn_clicked()
{

}


void TerminalView::on_sendButton_clicked()
{

}


void TerminalView::on_pushButton_2_clicked()
{
    QByteArray array;

    array.append(getCmdHead()+"dmrat,AT+dmrpSetcontact=2,2,3100,");

    QByteArray array_text;


    QTextCodec *code = QTextCodec::codecForName(m_codecName);

    array_text =code->fromUnicode(ui->apnUser->text().toUtf8());
    if (0==array_text.length()) {
        array.append("6888999,");
    } else {
        bool ok;

        // Convert to int
        int value = array_text.toInt(&ok); // base 10 by default

        if (ok && value>0 && value<16777215) {
            array.append(QString::number(value)+",");
        } else {
            array.append("6888999,");
        }
    }
    array.append("\r\n");
    sendDataRequestEx(array);

    QTimer::singleShot(300, this, [this]() {
        QByteArray array;
        array.clear();
        int randomValue = QRandomGenerator::global()->bounded(16777215);
        array.append(getCmdHead()+"dmrat,AT+DMRRadioId=" + QString::number(randomValue)+",\r\n");
        sendDataRequestEx(array);
    });

}

