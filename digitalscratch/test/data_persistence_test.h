#include <QObject>
#include <QtTest>

class Data_persistence_Test : public QObject
{
    Q_OBJECT

 public:
    Data_persistence_Test();

 private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
};
