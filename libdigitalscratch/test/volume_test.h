#include <QObject>
#include <QtTest>
#include <iostream>
using namespace std;

class Volume_Test : public QObject
{
    Q_OBJECT

public:
    Volume_Test();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void testCase_constructor();
};
