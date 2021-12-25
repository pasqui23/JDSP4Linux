#include <IAudioService.h>

#include "SettingsFragment.h"
#include "ui_MainWindow.h"
#include "ui_SettingsFragment.h"

#include "config/AppConfig.h"
#include "interface/dialog/PaletteEditor.h"
#include "interface/QMenuEditor.h"
#include "interface/TrayIcon.h"
#include "MainWindow.h"
#include "utils/AutoStartManager.h"

#include <QCloseEvent>
#include <QDebug>
#include <QDesktopServices>
#include <QGraphicsOpacityEffect>
#include <QMessageBox>
#include <QProcess>
#include <QStyleFactory>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
using namespace std;
static bool lockslot = false;

SettingsFragment::SettingsFragment(TrayIcon *trayIcon,
                                   IAudioService *audioService,
                                   QWidget  *parent) :
    BaseFragment(parent),
	ui(new Ui::SettingsFragment),
    _trayIcon(trayIcon),
    _audioService(audioService)
{
	ui->setupUi(this);

#ifdef USE_PULSEAUDIO
    ui->devices->setEnabled(false);
    ui->devices_group->setTitle("Select sink/device to be processed (PipeWire only)");
    ui->blocklistBox->setVisible(false);
#endif

    QString autostart_path = AutostartManager::getAutostartPath("jdsp-gui.desktop");

	/*
	 * Prepare TreeView
	 */

    ui->selector->setCurrentItem(ui->selector->topLevelItem(0));
	ui->stackedWidget->setCurrentIndex(0);
    ui->stackedWidget->repaint();
	connect(ui->selector, static_cast<void (QTreeWidget::*)(QTreeWidgetItem*, QTreeWidgetItem*)>(&QTreeWidget::currentItemChanged), this, [this](QTreeWidgetItem *cur, QTreeWidgetItem*)
	{
		int toplevel_index = ui->selector->indexOfTopLevelItem(cur);

		switch (toplevel_index)
		{
			case -1:
                if (cur->text(0) == "Context menu")
				{
                    ui->stackedWidget->setCurrentIndex(4);
				}
				break;
			default:
                ui->stackedWidget->setCurrentIndex(toplevel_index);
                break;
		}

        // Workaround: Force redraw
        ui->stackedWidget->hide();
        ui->stackedWidget->show();
        ui->stackedWidget->repaint();
	});
    ui->selector->expandItem(ui->selector->findItems("Tray icon", Qt::MatchFlag::MatchExactly).first());

	/*
	 * Prepare all combooxes
	 */
	ui->paletteSelect->addItem("Default",    "default");
	ui->paletteSelect->addItem("Black",      "black");
	ui->paletteSelect->addItem("Blue",       "blue");
	ui->paletteSelect->addItem("Dark",       "dark");
	ui->paletteSelect->addItem("Dark Blue",  "darkblue");
	ui->paletteSelect->addItem("Dark Green", "darkgreen");
	ui->paletteSelect->addItem("Honeycomb",  "honeycomb");
	ui->paletteSelect->addItem("Gray",       "gray");
	ui->paletteSelect->addItem("Green",      "green");
	ui->paletteSelect->addItem("Stone",      "stone");
	ui->paletteSelect->addItem("Custom",     "custom");

    for ( const auto& i : QStyleFactory::keys())
	{
		ui->themeSelect->addItem(i);
	}

	/*
	 * Refresh all input fields
	 */
	refreshAll();


	/*
	 * Connect all signals for Session
	 */
	auto systray_sel = [this]
					   {
						   if (lockslot)
						   {
							   return;
						   }

                           AppConfig::instance().set(AppConfig::TrayIconEnabled, ui->systray_r_showtray->isChecked());
                           ui->systray_icon_box->setEnabled(ui->systray_r_showtray->isChecked());
                           ui->menu_edit->setEnabled(ui->systray_r_showtray->isChecked());
					   };
	connect(ui->systray_r_none,     &QRadioButton::clicked, this, systray_sel);
	connect(ui->systray_r_showtray, &QRadioButton::clicked, this, systray_sel);
	auto autostart_update = [this, autostart_path]
							{
								if (ui->systray_minOnBoot->isChecked())
								{
									AutostartManager::saveDesktopFile(autostart_path,
                                                                      AppConfig::instance().get<QString>(AppConfig::ExecutablePath),
									                                  ui->systray_delay->isChecked());
								}
								else
								{
									QFile(autostart_path).remove();
								}

								ui->systray_delay->setEnabled(ui->systray_minOnBoot->isChecked());
							};
    connect(ui->systray_minOnBoot,     &QPushButton::clicked, this, autostart_update);
	connect(ui->systray_delay,         &QPushButton::clicked, this, autostart_update);
	/*
	 * Connect all signals for Interface
	 */
    connect(ui->themeSelect, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index)
	{
		if (lockslot)
		{
		    return;
		}

        AppConfig::instance().set(AppConfig::Theme, ui->themeSelect->itemText(index).toUtf8().constData());
	});
    connect(ui->paletteSelect, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index)
	{
		if (lockslot)
		{
		    return;
		}

        AppConfig::instance().set(AppConfig::ThemeColors, ui->paletteSelect->itemData(index).toString());
        ui->paletteConfig->setEnabled(AppConfig::instance().get<QString>(AppConfig::ThemeColors) == "custom");
	});
	connect(ui->paletteConfig, &QPushButton::clicked, this, [this]
	{
		auto c = new class PaletteEditor (&AppConfig::instance(), this);
		c->setFixedSize(c->geometry().width(), c->geometry().height());
		c->show();
	});
	connect(ui->eq_alwaysdrawhandles, &QCheckBox::clicked, [this]()
	{
        AppConfig::instance().set(AppConfig::EqualizerShowHandles, ui->eq_alwaysdrawhandles->isChecked());
	});
	/*
	 * Connect all signals for Default Paths
	 */
	connect(ui->saveirspath, &QPushButton::clicked, this, [this]
	{
		AppConfig::instance().setIrsPath(ui->irspath->text());
	});
	connect(ui->saveddcpath, &QPushButton::clicked, this, [this]
	{
        AppConfig::instance().setVdcPath(ui->ddcpath->text());
	});
	connect(ui->saveliveprogpath, &QPushButton::clicked, this, [this]
	{
		AppConfig::instance().setLiveprogPath(ui->liveprog_path->text());
	});
    connect(ui->liveprog_autoextract, &QCheckBox::clicked, this, [this]()
	{
        AppConfig::instance().set(AppConfig::LiveprogAutoExtract, ui->liveprog_autoextract->isChecked());
	});
	connect(ui->liveprog_extractNow, &QPushButton::clicked, this, [this]
	{
        auto reply = QMessageBox::question(this, tr("Question"), tr("Do you want to override existing EEL scripts (if any)?"),
		                              QMessageBox::Yes | QMessageBox::No);

		emit requestEelScriptExtract(reply == QMessageBox::Yes, true);
	});

	/*
	 * Connect all signals for Devices
	 */
	auto deviceUpdated = [this]()
     {
         if (lockslot)
         {
             return;
         }

         AppConfig::instance().set(AppConfig::AudioOutputUseDefault, ui->dev_mode_auto->isChecked());

         if (!ui->dev_mode_auto->isChecked())
         {
             if (ui->dev_select->currentData() == "---")
             {
                 return;
             }

             AppConfig::instance().set(AppConfig::AudioOutputDevice, ui->dev_select->currentData());
         }
     };

	connect(ui->dev_mode_auto,   &QRadioButton::clicked,                                                             this, deviceUpdated);
	connect(ui->dev_mode_manual, &QRadioButton::clicked,                                                             this, deviceUpdated);
    connect(ui->dev_select,      qOverload<int>(&QComboBox::currentIndexChanged), this, deviceUpdated);

	/*
	 * Connect all signals for Global
	 */
    connect(ui->close,  &QPushButton::clicked, this, &SettingsFragment::closePressed);
	connect(ui->github, &QPushButton::clicked, this, [] {
		QDesktopServices::openUrl(QUrl("https://github.com/Audio4Linux/JDSP4Linux"));
	});
    connect(ui->menu_edit, &QMenuEditor::targetChanged, this, [this, trayIcon]
	{
		auto menu = ui->menu_edit->exportMenu();
		trayIcon->updateTrayMenu(menu);
	});
    connect(ui->menu_edit, &QMenuEditor::resetPressed, this, [this, trayIcon]
	{
		QMessageBox::StandardButton reply = QMessageBox::question(this, "Warning", "Do you really want to restore the default layout?",
		                                                          QMessageBox::Yes | QMessageBox::No);

		if (reply == QMessageBox::Yes)
		{
		    ui->menu_edit->setTargetMenu(trayIcon->buildDefaultActions());
		    auto menu = ui->menu_edit->exportMenu();
		    trayIcon->updateTrayMenu(menu);
		}
	});
	ui->menu_edit->setSourceMenu(trayIcon->buildAvailableActions());

    connect(ui->run_first_launch, &QPushButton::clicked, this, [this]
    {
        emit closePressed();
        QTimer::singleShot(300, this, &SettingsFragment::launchSetupWizard);
    });
    connect(ui->blocklistClear, &QPushButton::clicked, this, []
    {
        AppConfig::instance().set(AppConfig::AudioAppBlocklist, "");
    });
    connect(ui->blocklistInvert, &QCheckBox::stateChanged, this, [this](bool state)
    {
        auto invertInfo = "You are about to enable allowlist mode. JamesDSP will not process all applications by default while this mode is active. "
                          "You need to explicitly allow each app to get processed in the 'Apps' menu.\n";
        auto button = QMessageBox::question(this, "Are you sure?",
                              state ? invertInfo : ""
                              "This action will reset your current blocklist or allowlist. Do you want to continue?");

        if(button == QMessageBox::Yes)
        {
            AppConfig::instance().set(AppConfig::AudioAppBlocklistInvert, state);
            AppConfig::instance().set(AppConfig::AudioAppBlocklist, "");
        }
    });
	/*
	 * Check for systray availability
	 */
#ifndef QT_NO_SYSTEMTRAYICON
	ui->systray_unsupported->hide();
#else
	ui->session->setEnabled(false);
#endif

	if (!QSystemTrayIcon::isSystemTrayAvailable())
	{
		ui->systray_unsupported->show();
		ui->session->setEnabled(false);
	}
}

SettingsFragment::~SettingsFragment()
{
	delete ui;
}

void SettingsFragment::refreshDevices()
{
	lockslot = true;
	ui->dev_select->clear();

    ui->dev_mode_auto->setChecked(AppConfig::instance().get<bool>(AppConfig::AudioOutputUseDefault));
    ui->dev_mode_manual->setChecked(!AppConfig::instance().get<bool>(AppConfig::AudioOutputUseDefault));

    auto devices = _audioService->sinkDevices();

    ui->dev_select->addItem("...", 0);
    for (const auto& device : devices)
    {
        ui->dev_select->addItem(QString("%1 (%2)")
                                .arg(QString::fromStdString(device.description))
                                .arg(QString::fromStdString(device.name)), QString::fromStdString(device.name));
    }

    auto current = AppConfig::instance().get<QString>(AppConfig::AudioOutputDevice);

    bool notFound = true;

    for (int i = 0; i < ui->dev_select->count(); i++)
    {
        if (ui->dev_select->itemData(i) == current)
        {
            notFound = false;
            ui->dev_select->setCurrentIndex(i);
            break;
        }
    }

    if (notFound)
    {
        QString name = QString("Unknown (%1)").arg(current);
        ui->dev_select->addItem(name, current);
        ui->dev_select->setCurrentText(name);
    }
	lockslot = false;
}

void SettingsFragment::refreshAll()
{
	lockslot = true;
	QString autostart_path = AutostartManager::getAutostartPath("jdsp-gui.desktop");

	ui->menu_edit->setTargetMenu(_trayIcon->getTrayMenu());
    ui->menu_edit->setIconStyle(AppConfig::instance().get<bool>(AppConfig::ThemeColorsCustomWhiteIcons));

	ui->irspath->setText(AppConfig::instance().getIrsPath());
    ui->ddcpath->setText(AppConfig::instance().getVdcPath());
	ui->liveprog_path->setText(AppConfig::instance().getLiveprogPath());

    ui->liveprog_autoextract->setChecked(AppConfig::instance().get<bool>(AppConfig::LiveprogAutoExtract));

    QString qvT(AppConfig::instance().get<QString>(AppConfig::Theme));
	int     indexT = ui->themeSelect->findText(qvT);

	if ( indexT != -1 )
	{
		ui->themeSelect->setCurrentIndex(indexT);
	}
	else
	{
		int index_fallback = ui->themeSelect->findText("Fusion");

		if ( index_fallback != -1 )
		{
			ui->themeSelect->setCurrentIndex(index_fallback);
		}
	}

    QVariant qvS2(AppConfig::instance().get<QString>(AppConfig::ThemeColors));
	int      index2 = ui->paletteSelect->findData(qvS2);

	if ( index2 != -1 )
	{
		ui->paletteSelect->setCurrentIndex(index2);
	}

    ui->paletteConfig->setEnabled(AppConfig::instance().get<QString>(AppConfig::ThemeColors) == "custom");

    ui->systray_r_none->setChecked(!AppConfig::instance().get<bool>(AppConfig::TrayIconEnabled));
    ui->systray_r_showtray->setChecked(AppConfig::instance().get<bool>(AppConfig::TrayIconEnabled));
    ui->systray_icon_box->setEnabled(AppConfig::instance().get<bool>(AppConfig::TrayIconEnabled));
    ui->menu_edit->setEnabled(AppConfig::instance().get<bool>(AppConfig::TrayIconEnabled));

	bool autostart_enabled     = AutostartManager::inspectDesktopFile(autostart_path, AutostartManager::Exists);
	bool autostart_delayed     = AutostartManager::inspectDesktopFile(autostart_path, AutostartManager::Delayed);

    ui->systray_minOnBoot->setChecked(autostart_enabled);
	ui->systray_delay->setEnabled(autostart_enabled);
	ui->systray_delay->setChecked(autostart_delayed);

    ui->eq_alwaysdrawhandles->setChecked(AppConfig::instance().get<bool>(AppConfig::EqualizerShowHandles));

    ui->blocklistInvert->setChecked(AppConfig::instance().get<bool>(AppConfig::AudioAppBlocklistInvert));

	refreshDevices();

    lockslot = false;
}

void SettingsFragment::updateButtonStyle(bool white)
{
	ui->menu_edit->setIconStyle(white);
}

void SettingsFragment::setVisible(bool visible)
{
	refreshDevices();
    BaseFragment::setVisible(visible);
}
