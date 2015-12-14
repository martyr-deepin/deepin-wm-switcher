#include <glib.h>
#include <iostream>
#include <string>
#include <vector>

#include <QtGlobal>
#include <QtGui>
#include <QX11Info>
#include <QtDBus>

#include <X11/Xlib-xcb.h>
#include <X11/keysym.h>
#include <xcb/xcb_keysyms.h>

#include "config.h"

#define C2Q(cs) (QString::fromUtf8((cs).c_str()))

using namespace std;

#if USE_BUILTIN_KEYBINDING
// borrowed from vlc
static unsigned GetModifier( xcb_connection_t *p_connection, xcb_key_symbols_t *p_symbols, xcb_keysym_t sym )
{
    static const unsigned pi_mask[8] = {
        XCB_MOD_MASK_SHIFT, XCB_MOD_MASK_LOCK, XCB_MOD_MASK_CONTROL,
        XCB_MOD_MASK_1, XCB_MOD_MASK_2, XCB_MOD_MASK_3,
        XCB_MOD_MASK_4, XCB_MOD_MASK_5
    };

    if( sym == 0 )
        return 0; /* no modifier */

    const xcb_keycode_t *p_keys = xcb_key_symbols_get_keycode( p_symbols, sym );
    if( !p_keys )
        return 0;

    int i = 0;
    bool no_modifier = true;
    while( p_keys[i] != XCB_NO_SYMBOL )
    {
        if( p_keys[i] != 0 )
        {
            no_modifier = false;
            break;
        }
        i++;
    }

    if( no_modifier )
        return 0;

    xcb_get_modifier_mapping_cookie_t r =
        xcb_get_modifier_mapping( p_connection );
    xcb_get_modifier_mapping_reply_t *p_map =
        xcb_get_modifier_mapping_reply( p_connection, r, NULL );
    if( !p_map )
        return 0;

    xcb_keycode_t *p_keycode = xcb_get_modifier_mapping_keycodes( p_map );
    if( !p_keycode )
        return 0;

    for( int i = 0; i < 8; i++ )
        for( int j = 0; j < p_map->keycodes_per_modifier; j++ )
    for( int k = 0; p_keys[k] != XCB_NO_SYMBOL; k++ )
        if( p_keycode[i*p_map->keycodes_per_modifier + j] == p_keys[k])
        {
            free( p_map );
            return pi_mask[i];
        }

    free( p_map ); // FIXME to check
    return 0;
}
#endif

#ifndef __alpha__
#define wmm_debug() qDebug()
#define wmm_warning() qWarning()
#define wmm_info() qInfo()
#else
#define wmm_debug() QMessageLogger(__FILE__, __LINE__, Q_FUNC_INFO).debug() 
#define wmm_warning() QMessageLogger(__FILE__, __LINE__, Q_FUNC_INFO).warning() 
#define wmm_info() QMessageLogger(__FILE__, __LINE__, Q_FUNC_INFO).info() 
#endif

namespace wmm {
    struct WindowManager {
        string genericName;
        string execName;
    };

    using WindowManagerList = vector<WindowManager>;

    WindowManagerList wms = {
        {"deepin wm", "deepin-wm"},
        {"deepin metacity", "deepin-metacity"},
    };

    enum SwitchingPermission {
        ALLOW_NONE,
        ALLOW_TO_2D,
        ALLOW_TO_3D,
        ALLOW_BOTH
    };

    using WMPointer = WindowManagerList::iterator;

    static WMPointer good_wm = wms.begin();
    static WMPointer bad_wm = wms.begin() + 1;
    static SwitchingPermission  switch_permission = ALLOW_NONE;

    static WindowManagerList::iterator apply_rules();

#if USE_BUILTIN_KEYBINDING
    class MyShortcutManager: public QObject, public QAbstractNativeEventFilter {
        Q_OBJECT
        public:
            MyShortcutManager() {
                static const xcb_keysym_t ignored_modifiers[] = {
                    0,
                    XK_Num_Lock,
                    XK_Scroll_Lock,
                    XK_Caps_Lock,
                };

                auto* c = QX11Info::connection();

                xcb_key_symbols_t *keysyms = xcb_key_symbols_alloc(c);
                _keycodes = xcb_key_symbols_get_keycode(keysyms, XK_P);

                if (!_keycodes || _keycodes[0] == XCB_NO_SYMBOL) {
                    wmm_warning() << ("keycode is invalid");
                }

                _mods = XCB_MOD_MASK_SHIFT | XCB_MOD_MASK_CONTROL;

                for (int k = 0, max = sizeof(ignored_modifiers)/sizeof(ignored_modifiers); k < max; k++) {
                    unsigned ignored_modifier = GetModifier(c, keysyms, ignored_modifiers[k]);
                    if (k != 0 && ignored_modifier == 0) continue;

                    int i = 0;
                    while (_keycodes && _keycodes[i] != XCB_NO_SYMBOL) {
                        registerShortcut(_keycodes[i], _mods | ignored_modifier);
                        i++;
                    }
                }

                xcb_key_symbols_free(keysyms);
            }

            ~MyShortcutManager() {
                int i = 0;
                while (_keycodes && _keycodes[i] != XCB_NO_SYMBOL) {
                    unregisterShortcut(_keycodes[i], _mods);
                    i++;
                }
            }

            virtual bool nativeEventFilter(const QByteArray &eventType, void *message, long *) Q_DECL_OVERRIDE {
                if (eventType == "xcb_generic_event_t") {
                    xcb_generic_event_t* ev = static_cast<xcb_generic_event_t *>(message);
                    switch (ev->response_type & ~0x80) {
                        case XCB_KEY_PRESS: {
                            xcb_key_press_event_t *kev = (xcb_key_press_event_t *)ev;
                            int i = 0;
                            while (_keycodes && _keycodes[i] != XCB_NO_SYMBOL) {
                                if (kev->detail == _keycodes[i] && (kev->state & _mods)) {
                                    wmm_info() << "shortcut triggered";
                                    emit toggleWM();
                                    break;
                                }
                            }
                            break;
                        }

                        case XCB_KEY_RELEASE: {
                            xcb_key_release_event_t *kev = (xcb_key_release_event_t *)ev;
                            break;
                        }

                        default: break;
                    }
                }
                return false;
            }

        signals:
            void toggleWM();

        private:
            xcb_keycode_t* _keycodes;
            uint16_t _mods;

            bool registerShortcut(xcb_keycode_t keycode, uint16_t mods) {
                xcb_screen_iterator_t iter;
                iter = xcb_setup_roots_iterator (xcb_get_setup (QX11Info::connection()));

                xcb_grab_key(QX11Info::connection(), 0, iter.data->root, mods,
                        keycode, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
            }

            void unregisterShortcut(xcb_keycode_t keycode, uint16_t mods) {
                xcb_ungrab_key(QX11Info::connection(), keycode, QX11Info::appRootWindow(), mods);
            }
    };

#else

    class MyRemoteRequestHandler: public QDBusAbstractAdaptor {
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "com.deepin.wm_switcher")
        public:
            MyRemoteRequestHandler(QObject* parent): QDBusAbstractAdaptor(parent) {
            }

        public slots:
            void requestSwitchWM() {
                emit toggleWM();
            }

        signals:
            void toggleWM();
    };

#endif


    class NotifyHelper: public QObject {
        Q_OBJECT
        public:
            void notifyStart3D() {
                exec(QString("%1 %2").arg(_cmd).arg("--SwitchWM3D"));
            }

            void notifyStart2D() {
                exec(QString("%1 %2").arg(_cmd).arg("--SwitchWM2D"));
            }

            void notify3DError() {
                exec(QString("%1 %2").arg(_cmd).arg("--SwitchWMError"));
            }

        private:
            QString _cmd {"/usr/lib/deepin-daemon/dde-osd"};

            void exec(QString program) {
                QProcess p;
                p.startDetached(program);
            }
    };

    class Rule {
        public:
            /**
             * return true if checker can confirm the use of specific  wm. 
             * else false to do next check.
             */
            virtual bool doTest(WMPointer base) = 0;
            virtual WMPointer getSupport() = 0;
    };

    class PlatformChecker: public Rule {
        public:
            bool doTest(WMPointer base) override {
                _voted = base;

                QProcess uname;
                uname.start("uname -m");
                if (uname.waitForStarted()) {
                    if (uname.waitForFinished()) {
                        auto data = uname.readAllStandardOutput();
                        string machine(data.trimmed().constData());
                        wmm_info() << QString("machine: %1").arg(machine.c_str());

                        QRegExp re("x86.*|i?86|ia64", Qt::CaseInsensitive);
                        if (re.exactMatch(C2Q(machine))) {
                            _voted = good_wm;
                            switch_permission = ALLOW_BOTH;

                        } else if (machine == "alpha") { // shenwei
                            _voted = bad_wm;
                            switch_permission = ALLOW_NONE;

                        } else if (machine.find("mips") != string::npos) { // loongson
                            //TODO: may need to check graphics card
                            _voted = good_wm;
                            switch_permission = ALLOW_BOTH;

                        } else {
                            // unknown machine
                            return false;
                        }

                        return true;
                    }
                }

                return false;
            }

            WMPointer getSupport() override {
                return _voted;
            }

        private:
            WMPointer _voted { wms.end() };
    };

    class EnvironmentChecker: public Rule {
        public:
            bool doTest(WMPointer base) override {
                _voted = base;
                return false;
            }

            WMPointer getSupport() override {
                return _voted;
            }

        private:
            WMPointer _voted { wms.end() };
    };

    class WindowManagerMonitor: public QObject {
        Q_OBJECT
        public:
            void start(const WindowManagerList::iterator& init_wm) {
                _current = init_wm;
                wmm_info() << QString("exec wm %1").arg(C2Q(_current->genericName));

                spawn();

                //QTimer::singleShot(CHECK_PERIOD, this, &WindowManagerMonitor::onTimeout);
            }

            virtual ~WindowManagerMonitor() {
                if (_proc) delete _proc;
            }

        public slots:
            void onToggleWM() {
                //TODO: setup rules
                wmm_debug() << __func__ << "switch_permission = " << switch_permission;
                switch (switch_permission) {
                    case ALLOW_NONE: {
                        if (_current == bad_wm) {
                            _notify.notify3DError();
                        }
                        return;
                    }
                    case ALLOW_BOTH: break;
                    case ALLOW_TO_2D: if (_current == bad_wm) return;
                    case ALLOW_TO_3D: if (_current == good_wm) return;
                }

                WMPointer old = _current;
                if (old == bad_wm) {
                    _current = good_wm;
                    _requestedNotify = &NotifyHelper::notifyStart3D;
                } else if (old == good_wm) {
                    _current = bad_wm;
                    _requestedNotify = &NotifyHelper::notifyStart2D;
                } else {
                    return;
                }
                spawn();
            }

        private:
            WMPointer _current { wms.end() };
            QProcess* _proc {nullptr};
            NotifyHelper _notify;
            int _spawnCount {0};

            using NotifyRequest = void (NotifyHelper::*)();
            NotifyRequest _requestedNotify {nullptr};

            const int CHECK_PERIOD = 1000;
            const int STARTUP_DELAY = 500;
            const int NOTIFY_DELAY = 600;
            const int KILL_TIMEOUT = 3000;

            void doSanityCheck() {
                auto old = _current;
                bool changed = false;

                QString s = QString::fromUtf8(qgetenv("PATH"));
                QString cmd = QStandardPaths::findExecutable(C2Q(good_wm->execName));
                QFileInfo fi_good(cmd);

                cmd = QStandardPaths::findExecutable(C2Q(bad_wm->execName));
                QFileInfo fi_bad(cmd);

                if (_current == good_wm) {
                    if (!fi_good.exists() || !fi_good.isExecutable()) {
                        _current = bad_wm;
                        if (!fi_bad.exists() || !fi_bad.isExecutable()) {
                            _current = wms.end();
                        }
                        changed = true;
                    }

                } else if (_current == bad_wm) {
                    if (!fi_bad.exists() || !fi_bad.isExecutable()) {
                        _current = good_wm;
                        if (!fi_good.exists() || !fi_good.isExecutable()) {
                            _current = wms.end();
                        }
                        changed = true;
                    }

                }

                if (changed) {
                    wmm_warning() << QString("%1: %2 -> %3").arg(__func__)
                        .arg(old != wms.end() ? old->genericName.c_str() : "")
                        .arg(_current != wms.end() ? _current->genericName.c_str() : "");
                    _requestedNotify = nullptr;
                }
            }

        private slots:
            void spawn() {
                if (_proc && _proc->state() == QProcess::Running) {
                    wmm_warning() << QString("%1 is running, force it to terminate").arg(_proc->program());
                    _proc->disconnect();
                    _proc->kill();
                    while (!_proc->waitForFinished(KILL_TIMEOUT)) {
                        _proc->kill();
                    }
                }

                if (!_proc) {
                    _proc = new QProcess;
                }

                doSanityCheck();
                if (_current == wms.end()) return;

                connect(_proc, SIGNAL(finished(int, QProcess::ExitStatus)), 
                            this, SLOT(onWMProcFinished(int, QProcess::ExitStatus)));
                _proc->start(C2Q(_current->execName), QStringList() << "--replace");

                if (!_proc->waitForStarted(STARTUP_DELAY)) {
                    wmm_warning() << QString("%1 start failed").arg(_proc->program());
                    if (switch_permission != ALLOW_BOTH && _current == good_wm) {
                        _requestedNotify = &NotifyHelper::notify3DError;
                    }
                } 

                QTimer::singleShot(NOTIFY_DELAY, this, SLOT(onDelayedNotify()));
                _spawnCount++;
            }

            void onDelayedNotify() {
                if (_requestedNotify != nullptr) {
                    (_notify.*_requestedNotify)();
                    _requestedNotify = nullptr;
                }
            }

            void onWMProcFinished(int exitCode, QProcess::ExitStatus status) {
                wmm_info() << __func__ << ": exitCode = " << exitCode;

                if (status == QProcess::CrashExit || exitCode != 0) {
                    wmm_warning() << QString("%1 crashed or failure, switch wm").arg(_proc->program());
                    _current = _current == good_wm ? bad_wm: good_wm;
                }

                QTimer::singleShot(STARTUP_DELAY, this, SLOT(spawn()));
            }

            void onTimeout() {
                if (_current == wms.end()) {
                    wmm_warning() << "there is no wm running currently, try launch one";
                    _current = apply_rules();
                    spawn();
                }

                QTimer::singleShot(CHECK_PERIOD, this, SLOT(onTimeout()));
            }

    };


    static WindowManagerList::iterator apply_rules() {
        vector<Rule*> rules = {
            new PlatformChecker(),
            new EnvironmentChecker(),
        };

        WindowManagerList::iterator p = good_wm;
        for (auto& rule: rules) {
            if (rule->doTest(p)) {
                p = rule->getSupport();
                break;
            }
        }

        if (p == wms.end()) {
            p = good_wm;
        }

        return p;
    }
};


using namespace wmm;

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

#if USE_BUILTIN_KEYBINDING
    wmm::MyShortcutManager xcbFilter;
    app.installNativeEventFilter(&xcbFilter);
#else
    wmm::MyRemoteRequestHandler dobj(&app);

    auto conn = QDBusConnection::sessionBus();
    if (!conn.registerService("com.deepin.wm_switcher")) {
        wmm_warning() << "register service failed";
        return -1;
    }

    conn.registerObject("/com/deepin/wm_switcher", &app);
#endif


    wmm::WindowManagerMonitor wmMonitor;

    auto p = wmm::apply_rules();
    wmMonitor.start(p);

#if USE_BUILTIN_KEYBINDING
    QObject::connect(&xcbFilter, SIGNAL(toggleWM()), &wmMonitor, SLOT(onToggleWM()));
#else
    QObject::connect(&dobj, SIGNAL(toggleWM()), &wmMonitor, SLOT(onToggleWM()));
#endif

    app.exec();

    return 0;
}

#include "main.moc"
