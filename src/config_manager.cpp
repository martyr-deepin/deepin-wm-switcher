#include "config.h"
#include "config_manager.h"

namespace wmm {

Config::Config() { load(); }

void Config::load() 
{
    if (_loaded) return;
    // global config
    {
        QString gpath("/etc/deepin-wm-switcher/config.json");
        if (QFile::exists(gpath)) {
            _global = loadFrom(gpath);
            if (!_global.isEmpty()) {
                wmm_info() << "global config exists";
            }
        }
    }

    // user level config
    {
        QString config_base = QStandardPaths::writableLocation( QStandardPaths::ConfigLocation);
        if (config_base.isEmpty()) {
            config_base = QString("%1/.config").arg(QDir::homePath());
        }

        _path = QString("%1/deepin/deepin-wm-switcher/config.json").arg(config_base);
        QDir dir(_path.path());
        if (dir.exists() || dir.mkpath(_path.path())) {
            _jobj = loadFrom(_path.absoluteFilePath());
            if (!_jobj.isEmpty()) {
                wmm_info() << "load config done";
            }
        } else {
            wmm_warning() << "config path does not exists or can not be made.";
        }
    }

    _loaded = true;
}

QJsonObject Config::loadFrom(const QString& path)
{
    QFile f(path);
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        QJsonParseError error;
        auto doc = QJsonDocument::fromJson(f.readAll(), &error);
        if (error.error != QJsonParseError::NoError) {
            wmm_warning() << error.errorString();
        }

        return doc.object();
    }

    return QJsonObject();
}

bool Config::save() 
{
    QDir dir(_path.path());
    if (!_path.exists() && !dir.mkpath(_path.path())) {
        return false;
    }

    QFile f(_path.filePath());
    if (f.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(_jobj);
        f.write(doc.toJson());
        f.flush();
    } else {
        wmm_warning() << "can not open config file to save";
    }

    return true;
}

QString Config::currentWM() 
{
    if (!_jobj.contains("last_wm")) {
        return _global["last_wm"].toString();
    }
    return _jobj["last_wm"].toString();
}

bool Config::allowSwitch() 
{
    if (!_jobj.contains("allow_switch")) {
        return _global["allow_switch"].toBool(true);
    } else {
        if (!_jobj["allow_switch"].isBool()) {
            _jobj["allow_switch"] = true;
        }
        return static_cast<QJsonObject&>(_jobj)["allow_switch"].toBool(true);
    }
}

void Config::setAllowSwitch(bool val) 
{
    _jobj["allow_switch"] = val;
    save();
}

void Config::selectWM(const QString& wm) 
{
    _jobj["last_wm"] = wm;
    save();
}

}

