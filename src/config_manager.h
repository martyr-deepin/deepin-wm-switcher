#pragma once

#include <QtCore>

namespace wmm {
class Config: public QObject {
    public:
        Config();

        void load(); 
        bool save();
        QString currentWM();

        bool allowSwitch();
        void setAllowSwitch(bool val);
        void selectWM(const QString& wm);

    private:
        QJsonObject _jobj;
        QJsonObject _global;
        QFileInfo _path;
        bool _loaded {false};

        QJsonObject loadFrom(const QString& path);
};
}
