#ifndef STREAMROWDELEGATE_H
#define STREAMROWDELEGATE_H

#include <QStyledItemDelegate>

namespace PlaylistUi {
  class StreamRowDelegate : public QStyledItemDelegate {
    Q_OBJECT

  public:
    explicit StreamRowDelegate(QObject *parent = nullptr, int heart_column = -1);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

  private:
    int heart_column = -1;
  };
}

#endif // STREAMROWDELEGATE_H
