#ifndef TERMINALVIEW_H
#define TERMINALVIEW_H

#include <QWidget>

namespace Ui {
class TerminalView;
}

class QTimer;
class QSettings;
class QByteArray;

class TerminalView : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalView(QWidget *parent = 0);
    ~TerminalView();

    void retranslate();
    void loadConfig(QSettings *config);
    void saveConfig(QSettings *config);
    void append(const QByteArray &array);
    void clear();
    void setFontFamily(QString fonts, int size, QString style);
    void setEnabled(bool status);
    void setPaused(bool status);
    void setHighlight(const QString &language);
    void setTextCodec(const QString &name);
    void setTabsInsertSpaces(bool enable);
    void setTabWidth(int width);
    void setAutoIndent(bool enable);
    void setIndentationGuides(bool enable);
    void saveText(const QString &fname);

signals:
    void sendDataRequest(const QByteArray &array);

private:
    void keyPressEvent(QKeyEvent  *event);
    void setSendButtonEnabled(bool status);
    void arrayToHex(QString &str, const QByteArray &arr, int countOfLine);
    void arrayToString(QString &str, const QByteArray &arr);
    void loadHistory(QSettings *config);
    void saveHistory(QSettings *config);

    void arrayToUTF8(QString &str, const QByteArray &array);
    void arrayToUTF16(QString &str, const QByteArray &array);
    void arrayToDualByte(QString &str, const QByteArray &array);
    void arrayToASCII(QString &str, const QByteArray &array);
    void sendDataRequestEx(const QByteArray &array);
    QString getCmdHead();
    void convertHexStr2Lcdmem(const QString &hex, int offset);
    void displayLcdScreen();
    void SaveBinFile();


private slots:
    void sendData();
    void onWrapBoxChanged(int status);
    void onSendButtonClicked();
    void updateResendTimerStatus();
    void setResendInterval(int msc);
    void onHistoryBoxChanged(const QString &string);

    void ondialButtonClicked();
    void ongpsRunClicked();
    void onlog2ButtonClicked();
    void onlog3ButtonClicked();
    void ontstButtonClicked();
    void onurlButtonClicked();
    void onpwoffButtonClicked();
    void onparamButtonClicked();
    void oncloselogClicked();
    void onimeiButtonClicked();
    void oniccidButtonClicked();
    void onwriteimeiClicked();
    void onhungButtonClicked();
    void onrebootButtonClicked();
    void ongpsNormalClicked();
    void onlog1ButtonClicked();
    void onapnButtonClicked();
    void onopenGpsClicked();
    void ongpsSignalClicked();
    void onuartButtonClicked();
    void onwlanButtonClicked();
    void onResetBntClicked();
    void onLogATClicked();
    void onLogDiagClicked();
    void ongetParamBtnClicked();
    void onlogFileBntClicked();
    void onlogOffBntClicked();
    void onLogOnBntClicked();
    void onsetBntClicked();
    void onUploadLogClicked();
    void onGetStateClicked();
    void onFlyOnClicked();
    void onFlyOffClicked();
    void onFactoryClicked();
    void onDeviceInfoClicked();

    void on_gpsLogOff_clicked();

    void on_download_clicked();

    void on_getApn_clicked();

    void on_sendCmd_clicked();

    void on_relogin_clicked();

    void on_keyMenu_clicked();

    void on_keyUp_clicked();

    void on_keyCancel_clicked();

    void on_keyLeft_clicked();

    void on_keyOk_clicked();

    void on_keyRight_clicked();

    void on_keyCall_clicked();

    void on_keyDown_clicked();

    void on_keyRed_clicked();

    void on_key1_clicked();

    void on_key2_clicked();

    void on_key3_clicked();

    void on_key5_clicked();

    void on_key4_clicked();

    void on_key6_clicked();

    void on_key7_clicked();

    void on_key8_clicked();

    void on_key9_clicked();

    void on_keyStar_clicked();

    void on_key0_clicked();

    void on_keyHash_clicked();

    void on_keyPtt_clicked();

    void on_keySos_clicked();

    void on_keyHome_clicked();

    void on_keyF1_clicked();

    void on_keyF2_clicked();

    void on_keyF3_clicked();

    void on_lcdfresh_clicked();

    void on_recInfo_clicked();

private:
    enum TextCodec {
        ASCII,
        GB2312,
        GB18030,
        UTF8,
        UTF16
    };

    Ui::TerminalView *ui;
    bool m_sendEnabled = false,  m_paused = false;
    QTimer *m_resendTimer;
    QByteArray *m_asciiBuf;
    enum TextCodec m_textCodec;
    QByteArray m_codecName;
    uchar *lcdbuf;
    QString lcdLineStr;
    int recvlcdline;
};

#endif // TERMINALVIEW_H
