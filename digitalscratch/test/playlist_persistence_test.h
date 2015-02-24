#include <QObject>
#include <QtTest>
#include "playlist.h"
#include "playlist_persistence.h"

class Playlist_persistence_Test : public QObject
{
    Q_OBJECT

 public:
    Playlist_persistence_Test();

 private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void testCaseReadM3u();
    void testCaseReadPls();
};
