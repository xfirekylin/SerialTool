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

TerminalView::TerminalView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TerminalView)
{
    ui->setupUi(this);

    m_resendTimer = new QTimer;
    m_asciiBuf = new QByteArray;

    ui->textEditRx->setReadOnly(true);
    ui->textEditTx->setWrap(true); // Send area auto wrap.

    setTextCodec("GB-2312"); // default

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
    } else {
         ui->textEditRx->append(string);
    }
    if (string.endsWith("\n")){
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

// send data
void TerminalView::sendData()
{
    QByteArray array;

    if (ui->portWriteAscii->isChecked() == true) {
        QTextCodec *code = QTextCodec::codecForName(m_codecName);
        array = code->fromUnicode(ui->textEditTx->text());
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
    array.append("AT##INRICO>reset\r\n");
    sendDataRequestEx(array);
}
void TerminalView::onLogATClicked(){
    QByteArray array;
    array.append("AT##INRICO>LogAT\r\n");
    sendDataRequestEx(array);

}
void TerminalView::onLogDiagClicked(){
    QByteArray array;
    array.append("AT##INRICO>LogDiag\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onUploadLogClicked(){
    QByteArray array;
    array.append("AT##INRICO>uploadLog\r\n");
    sendDataRequestEx(array);
}

 void TerminalView::onGetStateClicked(){
    QByteArray array;
    array.append("AT##INRICO>state\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onFlyOnClicked(){
    QByteArray array;
    array.append("AT##INRICO>fly1\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onFlyOffClicked(){
    QByteArray array;
    array.append("AT##INRICO>fly0\r\n");
    sendDataRequestEx(array);
}

void TerminalView::ongetParamBtnClicked(){
    QByteArray array;
    array.append("AT##INRICO>102\r\n");
    sendDataRequestEx(array);
}
void TerminalView::onlogFileBntClicked(){
    QByteArray array;
    array.append("AT##INRICO>LogFile\r\n");
    sendDataRequestEx(array);
}
void TerminalView::onlogOffBntClicked(){
    QByteArray array;
    array.append("AT##INRICO>log0\r\n");
    sendDataRequestEx(array);
}
void TerminalView::onLogOnBntClicked(){
    QByteArray array;
    array.append("AT##INRICO>log1\r\n");
    sendDataRequestEx(array);
}
void TerminalView::onsetBntClicked(){
    QByteArray array;

    array.append("AT##INRICO>apn&");

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
    array.append("AT##INRICO>deviceInfo\r\n");
    sendDataRequestEx(array);
}

void TerminalView::onFactoryClicked(){
    QByteArray array;
    array.append("AT##INRICO>factory\r\n");
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

        // 历史记录下拉列表删除多余项
        while (ui->historyBox->count() >= 20) {
            ui->historyBox->removeItem(19);
        }
        // 数据写入历史记录下拉列表
        int i = ui->historyBox->findText(str);
        if (i != -1) { // 存在的项先删除
            ui->historyBox->removeItem(i);
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
    array.append("AT##INRICO>Lvc1\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_download_clicked()
{
    QByteArray array;
    array.append("AT##INRICO>autoDl\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_getApn_clicked()
{
    QByteArray array;
    array.append("AT##INRICO>getApn\r\n");
    sendDataRequestEx(array);
}

void TerminalView::on_sendCmd_clicked()
{
    QByteArray array;

    QByteArray cmds = ui->cmdlist->currentText().toUtf8();
    if (cmds == "simKey"){
        array.append("AT##INRICO>simKey,2,100\r\n");
    }else if (cmds == "assertMode"){
        array.append("AT##INRICO>assertMode*I0,-1\r\n");
    }else if (cmds == "at2poc"){
        array.append("AT##INRICO>poc&AT+l\r\n");
    }else if (cmds == "at2UI"){
        array.append("AT##INRICO>poc>+atrsp\r\n");
    }else if (cmds == "Account"){
        array.append("AT##INRICO>t1&1185202675569392&123&zh.inrico.cn\r\n");
    } else {
        array.append("AT##INRICO>" +cmds+ "\r\n");
    }
    sendDataRequestEx(array);
}
