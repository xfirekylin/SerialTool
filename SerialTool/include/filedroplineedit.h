#ifndef FILEDROPLINEEDIT_H
#define FILEDROPLINEEDIT_H

#include <QLineEdit>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

class FileDropLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit FileDropLineEdit(QWidget *parent = nullptr) : QLineEdit(parent)
    {
        // Enable drop events for this widget
        setAcceptDrops(true);
        this->setText("112.74.76.236");
        setPlaceholderText("Drag and drop a file here...");
    }

protected:
    // Triggered when the user drags something over the widget
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction(); // Accept the drag if it's a file/URL
        } else {
            QLineEdit::dragEnterEvent(event);
        }
    }

    // Triggered when the user releases the mouse over the widget
    void dropEvent(QDropEvent *event) override
    {

            QStringList pathList;
            QList<QUrl> urlList = mimeData->urls();

            // Extract all file paths from the drop
            for (int i = 0; i < urlList.size(); ++i) {
                pathList.append(urlList.at(i).toLocalFile());
            }

            // Join them with a separator (e.g., a semicolon and a space)
            QString allPaths = pathList.join("; ");

            // Append to existing text or replace it
            if (this->text().isEmpty()) {
                this->setText(allPaths);
            } else {
                this->setText(this->text() + "; " + allPaths);
            }

            event->acceptProposedAction();
    }
};

#endif // FILEDROPLINEEDIT_H
