#include "QGCToolWidget.h"
#include "ui_QGCToolWidget.h"
#include "logging.h"
#include "QGCParamSlider.h"
#include "QGCComboBox.h"
#include "QGCTextLabel.h"
#include "QGCCommandButton.h"
#include "UASManager.h"

#include <QMenu>
#include <QList>
#include <QInputDialog>
#include <QDockWidget>
#include <QContextMenuEvent>
#include <QSettings>
#include <QFileDialog>
#include <QDesktopServices>

QGCToolWidget::QGCToolWidget(const QString& title, QWidget *parent, QSettings* settings) :
        QWidget(parent),
        mav(NULL),
        mainMenuAction(NULL),
        widgetTitle(title),
        ui(new Ui::QGCToolWidget)
{
    isFromMetaData = false;
    ui->setupUi(this);
    if (settings) loadSettings(*settings);

    if (title == "Unnamed Tool")
    {
        widgetTitle = QString("%1 %2").arg(title).arg(QGCToolWidget::instances()->count());
    }
    //QLOG_DEBUG() << "WidgetTitle" << widgetTitle;

    setObjectName(widgetTitle);
    createActions();
    toolLayout = ui->toolLayout;
    toolLayout->setAlignment(Qt::AlignTop);
    toolLayout->setSpacing(8);

    QDockWidget* dock = dynamic_cast<QDockWidget*>(this->parentWidget());
    if (dock) {
        dock->setWindowTitle(widgetTitle);
        dock->setObjectName(widgetTitle+"DOCK");
    }

    // Try with parent
    dock = dynamic_cast<QDockWidget*>(parent);
    if (dock) {
        dock->setWindowTitle(widgetTitle);
        dock->setObjectName(widgetTitle+"DOCK");
    }

    this->setWindowTitle(widgetTitle);
    QList<UASInterface*> systems = UASManager::instance()->getUASList();
    foreach (UASInterface* uas, systems)
    {
        UAS* newMav = dynamic_cast<UAS*>(uas);
        if (newMav)
        {
            addUAS(uas);
        }
    }
    connect(UASManager::instance(), SIGNAL(UASCreated(UASInterface*)), this, SLOT(addUAS(UASInterface*)));
    if (!instances()->contains(widgetTitle)) instances()->insert(widgetTitle, this);

    // Enforce storage if this not loaded from settings
    // is MUST NOT BE SAVED if it was loaded from settings!
    //if (!settings) storeWidgetsToSettings();
}

QGCToolWidget::~QGCToolWidget()
{
    if (mainMenuAction) mainMenuAction->deleteLater();
    if (QGCToolWidget::instances()) QGCToolWidget::instances()->remove(widgetTitle);
    delete ui;
}

void QGCToolWidget::setParent(QWidget *parent)
{
    QWidget::setParent(parent);
    // Try with parent
    QDockWidget* dock = dynamic_cast<QDockWidget*>(parent);
    if (dock)
    {
        dock->setWindowTitle(getTitle());
        dock->setObjectName(getTitle()+"DOCK");
    }
}

/**
 * @param parent Object later holding these widgets, usually the main window
 * @return List of all widgets
 */
QList<QGCToolWidget*> QGCToolWidget::createWidgetsFromSettings(QWidget* parent, QString settingsFile)
{
    // Load widgets from application settings
    QSettings* settings;

    // Or load them from a settings file
    if (!settingsFile.isEmpty())
    {
        settings = new QSettings(settingsFile, QSettings::IniFormat);
       //QLOG_DEBUG() << "LOADING SETTINGS FROM" << settingsFile;
    }
    else
    {
        settings = new QSettings();
        //QLOG_DEBUG() << "LOADING SETTINGS FROM DEFAULT" << settings->fileName();
    }

    QList<QGCToolWidget*> newWidgets;
    settings->beginGroup("Custom_Tool_Widgets");
    int size = settings->beginReadArray("QGC_TOOL_WIDGET_NAMES");
    for (int i = 0; i < size; i++)
    {
        settings->setArrayIndex(i);
        QString name = settings->value("TITLE", "").toString();
        QString objname = settings->value("OBJECT_NAME", "").toString();

        if (!instances()->contains(name) && name.length() != 0)
        {
            //QLOG_DEBUG() << "CREATED WIDGET:" << name;
            QGCToolWidget* tool = new QGCToolWidget(name, parent, settings);
            tool->setObjectName(objname);
            newWidgets.append(tool);
        }
        else if (name.length() == 0)
        {
            // Silently catch empty widget name - sanity check
            // to survive broken settings (e.g. from user manipulation)
        }
        else
        {
            //QLOG_DEBUG() << "WIDGET" << name << "DID ALREADY EXIST, REJECTING";
        }
    }
    settings->endArray();

    //QLOG_DEBUG() << "NEW WIDGETS: " << newWidgets.size();

    // Load individual widget items
    for (int i = 0; i < newWidgets.size(); i++)
    {
        newWidgets.at(i)->loadSettings(*settings);
    }
    settings->endGroup();
    settings->sync();
    delete settings;

    return instances()->values();
}
void QGCToolWidget::showLabel(QString name,int num)
{
    for (int i=0;i<toolItemList.size();i++)
    {
        if (toolItemList[i]->objectName() == name)
        {
            QGCTextLabel *label = qobject_cast<QGCTextLabel*>(toolItemList[i]);
            if (label)
            {
                label->enableText(num);
            }
        }
    }
}

/**
 * @param singleinstance If this is set to true, the widget settings will only be loaded if not another widget with the same title exists
 */
bool QGCToolWidget::loadSettings(const QString& settings, bool singleinstance)
{
    QSettings set(settings, QSettings::IniFormat);
    QStringList groups = set.childGroups();
    if (groups.length() > 0)
    {
        QString widgetName = groups.first();
	this->setObjectName(widgetName);
        if (singleinstance && QGCToolWidget::instances()->keys().contains(widgetName)) return false;
        // Do not use setTitle() here,
        // interferes with loading settings
        widgetTitle = widgetName;
        //QLOG_DEBUG() << "WIDGET TITLE LOADED: " << widgetName;
        loadSettings(set);
        return true;
    }
    else
    {
        return false;
    }
}
void QGCToolWidget::setSettings(QVariantMap& settings)
{
    isFromMetaData = true;
    settingsMap = settings;
    QString widgetName = getTitle();
    int size = settingsMap["count"].toInt();
    for (int j = 0; j < size; j++)
    {
        QString type = settings.value(widgetName + "\\" + QString::number(j) + "\\" + "TYPE", "UNKNOWN").toString();
        if (type == "SLIDER")
        {
            QString checkparam = settingsMap.value(widgetName + "\\" + QString::number(j) + "\\" + "QGC_PARAM_SLIDER_PARAMID").toString();
            paramList.append(checkparam);
        }
        else if (type == "COMBO")
        {
            QString checkparam = settingsMap.value(widgetName + "\\" + QString::number(j) + "\\" + "QGC_PARAM_COMBOBOX_PARAMID").toString();
            paramList.append(checkparam);
        }
    }
}
QList<QString> QGCToolWidget::getParamList()
{
    return paramList;
}
void QGCToolWidget::setParameterValue(int uas, int component, QString parameterName, const QVariant value)
{
    Q_UNUSED(uas)
    Q_UNUSED(component)
    Q_UNUSED(value)

    QString widgetName = getTitle();
    int size = settingsMap["count"].toInt();
    if (paramToItemMap.contains(parameterName))
    {
        //If we already have an item for this parameter, updates are handled internally.
        return;
    }

    for (int j = 0; j < size; j++)
    {
        QString type = settingsMap.value(widgetName + "\\" + QString::number(j) + "\\" + "TYPE", "UNKNOWN").toString();
        QGCToolWidgetItem* item = NULL;
        if (type == "COMMANDBUTTON")
        {
            //This shouldn't happen, but I'm not sure... so lets test for it.
            continue;
        }
        else if (type == "SLIDER")
        {
            QString checkparam = settingsMap.value(widgetName + "\\" + QString::number(j) + "\\" + "QGC_PARAM_SLIDER_PARAMID").toString();
            if (checkparam == parameterName)
            {
                item = new QGCParamSlider(this);
                paramToItemMap[parameterName] = item;
                addToolWidget(item);
                item->readSettings(widgetName + "\\" + QString::number(j) + "\\",settingsMap);
                return;
            }
        }
        else if (type == "COMBO")
        {
            QString checkparam = settingsMap.value(widgetName + "\\" + QString::number(j) + "\\" + "QGC_PARAM_COMBOBOX_PARAMID").toString();
            if (checkparam == parameterName)
            {
                item = new QGCComboBox(this);
                addToolWidget(item);
                item->readSettings(widgetName + "\\" + QString::number(j) + "\\",settingsMap);
                paramToItemMap[parameterName] = item;
                return;
            }
        }
    }
}

void QGCToolWidget::loadSettings(QVariantMap& settings)
{

    QString widgetName = getTitle();
    //settings.beginGroup(widgetName);
    QLOG_DEBUG() << "LOADING FOR" << widgetName;
    //int size = settings.beginReadArray("QGC_TOOL_WIDGET_ITEMS");
    int size = settings["count"].toInt();
    QLOG_DEBUG() << "CHILDREN SIZE:" << size;
    for (int j = 0; j < size; j++)
    {
        QApplication::processEvents();
        //settings.setArrayIndex(j);
        QString type = settings.value(widgetName + "\\" + QString::number(j) + "\\" + "TYPE", "UNKNOWN").toString();
        if (type != "UNKNOWN")
        {
            QGCToolWidgetItem* item = NULL;
            if (type == "COMMANDBUTTON")
            {
                item = new QGCCommandButton(this);
                //QLOG_DEBUG() << "CREATED COMMANDBUTTON";
            }
            else if (type == "TEXT")
            {
                item = new QGCTextLabel(this);
                item->setActiveUAS(mav);
            }
            else if (type == "SLIDER")
            {
                item = new QGCParamSlider(this);
                //QLOG_DEBUG() << "CREATED PARAM SLIDER";
            }
            else if (type == "COMBO")
            {
                item = new QGCComboBox(this);
                //QLOG_DEBUG() << "CREATED PARAM COMBOBOX";
            }
            if (item)
            {
                // Configure and add to layout
                addToolWidget(item);
                item->readSettings(widgetName + "\\" + QString::number(j) + "\\",settings);

                //QLOG_DEBUG() << "Created tool widget";
            }
        }
        else
        {
            QLOG_DEBUG() << "UNKNOWN TOOL WIDGET TYPE" << type;
        }
    }
    //settings.endArray();
    //settings.endGroup();
}

void QGCToolWidget::loadSettings(QSettings& settings)
{
    QString widgetName = getTitle();
    settings.beginGroup(widgetName);
    //QLOG_DEBUG() << "LOADING FOR" << widgetName;
    int size = settings.beginReadArray("QGC_TOOL_WIDGET_ITEMS");
    //QLOG_DEBUG() << "CHILDREN SIZE:" << size;
    for (int j = 0; j < size; j++)
    {
        settings.setArrayIndex(j);
        QString type = settings.value("TYPE", "UNKNOWN").toString();
        if (type != "UNKNOWN")
        {
            QGCToolWidgetItem* item = NULL;
            if (type == "COMMANDBUTTON")
            {
                QGCCommandButton *button = new QGCCommandButton(this);
                connect(button,SIGNAL(showLabel(QString,int)),this,SLOT(showLabel(QString,int)));
                item = button;
                item->setActiveUAS(mav);
                //QLOG_DEBUG() << "CREATED COMMANDBUTTON";
            }
            else if (type == "SLIDER")
            {
                item = new QGCParamSlider(this);
                item->setActiveUAS(mav);
                //QLOG_DEBUG() << "CREATED PARAM SLIDER";
            }
            else if (type == "COMBO")
            {
                item = new QGCComboBox(this);
                item->setActiveUAS(mav);
                QLOG_DEBUG() << "CREATED PARAM COMBOBOX";
            }
            else if (type == "TEXT")
            {
                item = new QGCTextLabel(this);
                item->setObjectName(settings.value("QGC_TEXT_ID").toString());
                item->setActiveUAS(mav);
            }

            if (item)
            {
                // Configure and add to layout
                addToolWidget(item);
                item->readSettings(settings);

                //QLOG_DEBUG() << "Created tool widget";
            }
        }
        else
        {
            //QLOG_DEBUG() << "UNKNOWN TOOL WIDGET TYPE";
        }
    }
    settings.endArray();
    settings.endGroup();
}

void QGCToolWidget::storeWidgetsToSettings(QString settingsFile)
{
    // Store list of widgets
    QSettings* settings;
    if (!settingsFile.isEmpty())
    {
        settings = new QSettings(settingsFile, QSettings::IniFormat);
        //QLOG_DEBUG() << "STORING SETTINGS TO" << settings->fileName();
    }
    else
    {
        settings = new QSettings();
        //QLOG_DEBUG() << "STORING SETTINGS TO DEFAULT" << settings->fileName();
    }

    settings->beginGroup("Custom_Tool_Widgets");
    int preArraySize = settings->beginReadArray("QGC_TOOL_WIDGET_NAMES");
    settings->endArray();

    settings->beginWriteArray("QGC_TOOL_WIDGET_NAMES");
    int num = 0;
    for (int i = 0; i < qMax(preArraySize, instances()->size()); ++i)
    {
        if (i < instances()->size())
        {
            // Updating value
            if (!instances()->values().at(i)->fromMetaData())
            {
                settings->setArrayIndex(num++);
                settings->setValue("TITLE", instances()->values().at(i)->getTitle());
                settings->setValue("OBJECT_NAME", instances()->values().at(i)->objectName());
                //QLOG_DEBUG() << "WRITING TITLE" << instances()->values().at(i)->getTitle();
            }
        }
        else
        {
            // Deleting old value
            settings->remove("TITLE");
        }
    }
    settings->endArray();

    // Store individual widget items
    for (int i = 0; i < instances()->size(); ++i)
    {
        instances()->values().at(i)->storeSettings(*settings);
    }
    settings->endGroup();
    settings->sync();
    delete settings;
}

void QGCToolWidget::storeSettings()
{
    QSettings settings;
    storeSettings(settings);
}

void QGCToolWidget::storeSettings(const QString& settingsFile)
{
    QSettings settings(settingsFile, QSettings::IniFormat);
    storeSettings(settings);
}

void QGCToolWidget::storeSettings(QSettings& settings)
{
    if (isFromMetaData)
    {
        //Refuse to store if this is loaded from metadata or dynamically generated.
        return;
    }
    //QLOG_DEBUG() << "WRITING WIDGET" << widgetTitle << "TO SETTINGS";
    settings.beginGroup(widgetTitle);
    settings.beginWriteArray("QGC_TOOL_WIDGET_ITEMS");
    int k = 0; // QGCToolItem counter
    for (int j = 0; j  < children().size(); ++j)
    {
        // Store only QGCToolWidgetItems
        QGCToolWidgetItem* item = dynamic_cast<QGCToolWidgetItem*>(children().at(j));
        if (item)
        {
            // Only count actual tool widget item children
            settings.setArrayIndex(k++);
            // Store the ToolWidgetItem
            item->writeSettings(settings);
        }
    }
    //QLOG_DEBUG() << "WROTE" << k << "SUB-WIDGETS TO SETTINGS";
    settings.endArray();
    settings.endGroup();
}

void QGCToolWidget::addUAS(UASInterface* uas)
{
    UAS* newMav = dynamic_cast<UAS*>(uas);
    if (newMav)
    {
        // FIXME Convert to list
        if (mav == NULL) mav = newMav;
    }
}

void QGCToolWidget::contextMenuEvent (QContextMenuEvent* event)
{
    QMenu menu(this);
    menu.addAction(addParamAction);
    menu.addAction(addCommandAction);
    menu.addSeparator();
    menu.addAction(setTitleAction);
    menu.addAction(exportAction);
    menu.addAction(importAction);
    menu.addAction(deleteAction);
    menu.exec(event->globalPos());
}

void QGCToolWidget::hideEvent(QHideEvent* event)
{
    // Store settings
    QWidget::hideEvent(event);
}

/**
 * The widgets current view and the applied dock widget area.
 * Both values are only stored internally and allow an external
 * widget to configure it accordingly
 */
void QGCToolWidget::setViewVisibilityAndDockWidgetArea(int view, bool visible, Qt::DockWidgetArea area)
{
    viewVisible.insert(view, visible);
    dockWidgetArea.insert(view, area);
}

void QGCToolWidget::createActions()
{
    addParamAction = new QAction(tr("New &Parameter Slider"), this);
    addParamAction->setStatusTip(tr("Add a parameter setting slider widget to the tool"));
    connect(addParamAction, SIGNAL(triggered()), this, SLOT(addParam()));

    addCommandAction = new QAction(tr("New MAV &Command Button"), this);
    addCommandAction->setStatusTip(tr("Add a new action button to the tool"));
    connect(addCommandAction, SIGNAL(triggered()), this, SLOT(addCommand()));

    setTitleAction = new QAction(tr("Set Widget Title"), this);
    setTitleAction->setStatusTip(tr("Set the title caption of this tool widget"));
    connect(setTitleAction, SIGNAL(triggered()), this, SLOT(setTitle()));

    deleteAction = new QAction(tr("Delete this widget"), this);
    deleteAction->setStatusTip(tr("Delete this widget permanently"));
    connect(deleteAction, SIGNAL(triggered()), this, SLOT(deleteWidget()));

    exportAction = new QAction(tr("Export this widget"), this);
    exportAction->setStatusTip(tr("Export this widget to be reused by others"));
    connect(exportAction, SIGNAL(triggered()), this, SLOT(exportWidget()));

    importAction = new QAction(tr("Import widget"), this);
    importAction->setStatusTip(tr("Import this widget from a file (current content will be removed)"));
    connect(importAction, SIGNAL(triggered()), this, SLOT(importWidget()));
}

QMap<QString, QGCToolWidget*>* QGCToolWidget::instances()
{
    static QMap<QString, QGCToolWidget*>* instances;
    if (!instances) instances = new QMap<QString, QGCToolWidget*>();
    return instances;
}

QList<QGCToolWidgetItem*>* QGCToolWidget::itemList()
{
    static QList<QGCToolWidgetItem*>* instances;
    if (!instances) instances = new QList<QGCToolWidgetItem*>();
    return instances;
}
void QGCToolWidget::addParam(int uas,int component,QString paramname,QVariant value)
{
    isFromMetaData = true;
    QGCParamSlider* slider = new QGCParamSlider(this);
    connect(slider, SIGNAL(destroyed()), this, SLOT(storeSettings()));
    if (ui->hintLabel)
    {
        ui->hintLabel->deleteLater();
        ui->hintLabel = NULL;
    }
    toolLayout->addWidget(slider);
    slider->setActiveUAS(mav);
    slider->setParameterValue(uas,component,0,-1,paramname,value);


}

void QGCToolWidget::addParam()
{
    QGCParamSlider* slider = new QGCParamSlider(this);
    connect(slider, SIGNAL(destroyed()), this, SLOT(storeSettings()));
    if (ui->hintLabel)
    {
        ui->hintLabel->deleteLater();
        ui->hintLabel = NULL;
    }
    toolLayout->addWidget(slider);
    slider->startEditMode();
}

void QGCToolWidget::addCommand()
{
    QGCCommandButton* button = new QGCCommandButton(this);
    connect(button, SIGNAL(destroyed()), this, SLOT(storeSettings()));
    if (ui->hintLabel)
    {
        ui->hintLabel->deleteLater();
        ui->hintLabel = NULL;
    }
    toolLayout->addWidget(button);
    button->startEditMode();
}

void QGCToolWidget::addToolWidget(QGCToolWidgetItem* widget)
{
    if (ui->hintLabel)
    {
        ui->hintLabel->deleteLater();
        ui->hintLabel = NULL;
    }
    connect(widget, SIGNAL(destroyed()), this, SLOT(storeSettings()));
    toolLayout->addWidget(widget);
    toolItemList.append(widget);
}

void QGCToolWidget::exportWidget()
{
    const QString widgetFileExtension(".qgw");
    QString fileName = QFileDialog::getSaveFileName(this, tr("Specify File Name"),
                                                    QGC::appDataDirectory(),
                                                    tr("Control Widget (*%1);;").arg(widgetFileExtension));
    if (!fileName.endsWith(widgetFileExtension))
    {
        fileName = fileName.append(widgetFileExtension);
    }
    storeSettings(fileName);
}

void QGCToolWidget::importWidget()
{
    const QString widgetFileExtension(".qgw");
    QString fileName = QFileDialog::getOpenFileName(this, tr("Specify File Name"),
                                                    QGC::appDataDirectory(),
                                                    tr("Control Widget (*%1);;").arg(widgetFileExtension));
    loadSettings(fileName);
}

const QString QGCToolWidget::getTitle()
{
    return widgetTitle;
}

void QGCToolWidget::setTitle()
{
    QDockWidget* parent = dynamic_cast<QDockWidget*>(this->parentWidget());
    if (parent)
    {
        bool ok;
        QString text = QInputDialog::getText(this, tr("Enter Widget Title"),
                                             tr("Widget title:"), QLineEdit::Normal,
                                             parent->windowTitle(), &ok);
        if (ok && !text.isEmpty())
        {
            setTitle(text);
        }
    }
}

void QGCToolWidget::setWindowTitle(const QString& title)
{
    // Sets title and calls setWindowTitle on QWidget
    widgetTitle = title;
    QWidget::setWindowTitle(title);
}

void QGCToolWidget::setTitle(QString title)
{
    // Remove references to old title
    /*QSettings settings;
    settings.beginGroup(widgetTitle);
    settings.remove("");
    settings.endGroup();
    settings.sync();*/

    if (instances()->contains(widgetTitle)) instances()->remove(widgetTitle);

    // Switch to new title
    widgetTitle = title;

    if (!instances()->contains(title)) instances()->insert(title, this);
    QWidget::setWindowTitle(title);
    QDockWidget* parent = dynamic_cast<QDockWidget*>(this->parentWidget());
    if (parent) parent->setWindowTitle(title);
    // Store all widgets
    //storeWidgetsToSettings();

    emit titleChanged(title);
    if (mainMenuAction) mainMenuAction->setText(title);
}

void QGCToolWidget::setMainMenuAction(QAction* action)
{
    this->mainMenuAction = action;
}

void QGCToolWidget::deleteWidget()
{
    // Remove from settings

    // Hide
    this->hide();
    instances()->remove(getTitle());
    /*QSettings settings;
    settings.beginGroup(getTitle());
    settings.remove("");
    settings.endGroup();
    storeWidgetsToSettings();*/
    storeWidgetsToSettings();

    // Delete
    this->deleteLater();
}
