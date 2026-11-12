#ifndef WIFIDIALOG_H
#define WIFIDIALOG_H

#include <QByteArray>
#include <QDialog>
#include <QVector>

class QComboBox;
class QPushButton;
class QShowEvent;
class QTableWidget;
class QTimer;

// 预置 WiFi 的开关、扫描和连接。协议与 linux3610paramcfg 的 WifiDlg 一致。
class WifiDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WifiDialog(const QString &cmdHead, const QByteArray &codecName, QWidget *parent = 0);
    void feedData(const QByteArray &data);

signals:
    void sendDataRequest(const QByteArray &data);

protected:
    void showEvent(QShowEvent *event);

private slots:
    void onSwitchChanged(int index);
    void onAdd();
    void onRefresh();
    void onConnectAp();
    void onDisconnectAp();
    void onSelectionChanged();
    void onHeaderClicked(int column);
    void onPollTimeout();
    void onScanTimeout();

private:
    struct ApRow {
        QByteArray ssid;
        int rssi;
        int connected;
    };

    void sendCmd(const QByteArray &body);
    void startPoll(int maxCount);
    void armScan();
    void handleLine(const QByteArray &line);
    void applyList();
    void fillList(const QByteArray &keepSsid);
    void sortRows();
    int compareRow(const ApRow &a, const ApRow &b) const;
    void updateButtons();
    void showFail();
    QString ssidText(const QByteArray &ssid) const;
    QByteArray encodeText(const QString &text) const;
    bool editWifi(const QString &title, const QString &okText, const QByteArray &ssidInit,
                  bool ssidReadOnly, QByteArray *ssidOut, QByteArray *pskOut);

    QString m_cmdHead;
    QByteArray m_codecName;
    QComboBox *m_combo;
    QTableWidget *m_list;
    QPushButton *m_connBtn;
    QPushButton *m_discBtn;
    QVector<ApRow> m_rows;
    QVector<ApRow> m_build;
    QByteArray m_rxBuf;
    bool m_collect;
    bool m_ignoreCombo;
    bool m_alive;
    bool m_started;
    int m_switchOn;
    int m_poll;
    int m_pollMax;
    int m_scanLeft;
    int m_sortCol;
    bool m_sortAsc;
    QTimer *m_pollTimer;
    QTimer *m_scanTimer;
};

#endif // WIFIDIALOG_H
