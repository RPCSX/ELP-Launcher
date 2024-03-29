#include "Context.hpp"
#include "FlowLayout.hpp"
#include "NativeLauncher.hpp"
#include "Widget.hpp"

#include <cstdio>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <qsettings.h>
#include <qstandardpaths.h>
#include <string_view>
#include <utility>

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QStringListModel>
#include <QSvgWidget>
#include <QToolButton>
#include <QValidator>
#include <QWidget>
#include <QXmlStreamReader>
#include <QtConcurrent>

static void showPackageSourcesDialog(
    Context &context, const MethodCallArgs &args,
    std::move_only_function<void(MethodCallResult)> responseHandler) {
  auto dialog = new QDialog(nullptr);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle(QString::fromStdString("Package sources"));
  dialog->setLayout(new QVBoxLayout(dialog));
  dialog->layout()->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  dialog->setMinimumWidth(600);

  auto addBtn = new QPushButton("Add", dialog);
  dialog->layout()->addWidget(addBtn);

  std::vector<QLineEdit *> inputLines;

  auto addInputLine = [&](std::string_view value) {
    auto line = new QGroupBox(dialog);
    line->setLayout(new QVBoxLayout(line));
    auto inputLine = new QHBoxLayout();

    auto edit = new QLineEdit(line);
    inputLines.push_back(edit);
    auto selectFolder = new QPushButton("Select folder", line);
    edit->setText(QString::fromUtf8(value));
    inputLine->addWidget(edit);
    inputLine->addWidget(selectFolder);

    line->layout()->addItem(inputLine);
    auto errorMessage = new QLabel(line);
    errorMessage->hide();
    line->layout()->addWidget(errorMessage);
    dialog->layout()->replaceWidget(addBtn, line);
    dialog->layout()->addWidget(addBtn);

    auto validate = [=](const QString &newText) {
      if (newText.isEmpty()) {
        selectFolder->show();
      } else {
        selectFolder->hide();
        auto url = Url(newText.toStdString());

        if (!url.valid()) {
          errorMessage->setText("wrong URL");
          errorMessage->show();
          return;
        }

        if (url.isLocalPath()) {
          auto localPath = url.toLocalPath();
          auto status = std::filesystem::status(localPath);
          if (status.type() != std::filesystem::file_type::directory &&
              status.type() != std::filesystem::file_type::regular) {
            errorMessage->setText("Local package not found");
            errorMessage->show();
            return;
          }
          if (status.type() == std::filesystem::file_type::directory) {
            if (!std::filesystem::is_regular_file(localPath /
                                                  "manifest.json")) {
              errorMessage->setText("Directory must contain manifest.json");
              errorMessage->show();
              return;
            }
          }
        }

        // TODO: provide more checks
      }

      errorMessage->hide();
    };

    validate(QString::fromUtf8(value));

    QObject::connect(selectFolder, &QPushButton::clicked, [=] {
      auto directory =
          QFileDialog::getExistingDirectoryUrl(nullptr, {}, edit->text());
      edit->setText(directory.toString());
    });

    QObject::connect(edit, &QLineEdit::textChanged, validate);
  };

  auto watchConnection = context.createNotificationHandler(
      "package-sources/change", [&](NotificationArgs args) {
        if (args.contains("add")) {
          for (auto &add : args["add"]) {
            addInputLine(add.get<std::string>());
          }
        }
      });

  std::set<std::string> prevLines;

  {
    auto &extSources =
        context.getSettings("package-sources", Settings::array());
    if (extSources.empty()) {
      addInputLine({});
    } else {
      for (auto &source : extSources) {
        auto line = source.get<std::string>();
        addInputLine(line);
        prevLines.insert(std::move(line));
      }
    }
  }

  QObject::connect(addBtn, &QPushButton::clicked, [&] { addInputLine({}); });
  QObject::connect(dialog, &QDialog::finished,
                   [&inputLines, prevLines = std::move(prevLines),
                    context = &context,
                    watchConnection = std::move(watchConnection)] mutable {
                     watchConnection.destroy();
                     std::set<std::string> newLines;
                     for (auto inputLine : inputLines) {
                       if (!inputLine->text().isEmpty()) {
                         newLines.insert(Url(inputLine->text()).toString());
                       }
                     }

                     std::vector<Url> removeSources;
                     std::vector<Url> addSources;

                     for (auto &line : prevLines) {
                       if (!newLines.contains(line)) {
                         removeSources.push_back(Url(line));
                       }
                     }

                     for (auto &line : newLines) {
                       if (!prevLines.contains(line)) {
                         addSources.push_back(Url(line));
                       }
                     }

                     context->editPackageSources(addSources, removeSources);
                   });
  dialog->exec();
  responseHandler({});
}

static void showAlternativeResolverDialog(
    Context &context, const MethodCallArgs &args,
    std::move_only_function<void(MethodCallResult)> responseHandler) {
  QStringListModel *model = new QStringListModel();

  auto dialog = new QDialog(nullptr);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setLayout(new QVBoxLayout(dialog));
  dialog->layout()->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  auto list = new QListView(dialog);
  dialog->layout()->addWidget(list);
  list->setModel(model);

  auto alwaysUse = new QCheckBox("Always use this alternative", dialog);
  dialog->layout()->addWidget(alwaysUse);

  auto downloadBtn = new QPushButton("Download package", dialog);
  dialog->layout()->addWidget(downloadBtn);

  if (!args.contains("groupId")) {
    alwaysUse->hide();
    dialog->setWindowTitle(QString::fromStdString("Select alternative"));
  } else {
    dialog->setWindowTitle(QString::fromStdString(
        "Select alternative for " + args["kind"].get<std::string>() +
        "::" + args["groupId"].get<std::string>()));
  }

  auto requirements = args["requirements"].get<AlternativeRequirements>();
  auto alternatives = args["alternatives"].get<std::vector<Manifest>>();
  QStringList alternativeList;
  for (auto &alt : alternatives) {
    alternativeList.push_back(QString::fromStdString(alt.id()));
  }
  model->setStringList(alternativeList);

  MethodCallResult response;
  QObject::connect(downloadBtn, &QPushButton::clicked, [&] {
    response = -1;
    dialog->accept();
  });

  QObject::connect(
      list, &QListView::doubleClicked, [&](const QModelIndex &index) {
        if (index.row() < 0) {
          return;
        }

        if (alwaysUse->isChecked()) {
          context.selectAlternative(
              args["kind"].get<std::string>(),
              args["groupId"].get<std::string>(),
              context.findAlternative(alternatives[index.row()].id()));
        }

        response = index.row();
        dialog->accept();
      });

  int result = dialog->exec();
  if (result == QDialog::Accepted) {
    responseHandler(std::move(response));
    return;
  }

  responseHandler({{"error", elp::ErrorCode::NotFound}});
}

static void showErrorDialog(
    Context &context, const MethodCallArgs &args,
    std::move_only_function<void(MethodCallResult)> responseHandler) {
  auto dialog = new QDialog();
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle(QString::fromStdString("Error Dialog"));
  dialog->resize(400, 300);
  dialog->setLayout(new QVBoxLayout(dialog));
  auto message = new QLabel(dialog);
  dialog->layout()->addWidget(message);

  if (args.is_string()) {
    message->setText(QString::fromStdString(args.get<std::string>()));
  } else if (args.is_number_integer()) {
    // FIXME: parse error code
    message->setText(
        QString::fromStdString(std::to_string(args.get<std::int64_t>())));
  } else {
    message->setText(QString::fromStdString(args.dump()));
  }

  dialog->exec();
  responseHandler({});
}

struct AlternativeViewWidget : QGroupBox {
  QPixmap pixmap;
  QSvgWidget *iconSvg;
  QLabel *icon;
  QLabel *label;
  QWidget *control;
  bool initialized = false;
  std::shared_ptr<Alternative> alternative;
  QFuture<void> initIconFuture;

  AlternativeViewWidget(Context &context, std::shared_ptr<Alternative> alt,
                        QWidget *parent)
      : QGroupBox(parent), alternative(std::move(alt)) {
    auto &manifest = alternative->manifest();
    setStyleSheet("QGroupBox { border: 1px solid palette(alternate-base); }");
    setLayout(new QVBoxLayout(this));
    setAttribute(Qt::WA_Hover);
    icon = new QLabel(this);
    icon->setFixedSize(200, 200);
    iconSvg = new QSvgWidget(this);
    iconSvg->setFixedSize(200, 200);
    iconSvg->hide();
    label = new QLabel(this);
    label->setAlignment(Qt::AlignCenter);

    label->setText(QString::fromStdString(manifest.displayId()));
    label->setWordWrap(true);
    control = new QWidget(this);
    control->setLayout(new QHBoxLayout(control));
    control->setObjectName("control");
    auto controlBackground = control->palette().color(QPalette::AlternateBase);
    controlBackground.setAlphaF(0.75);
    control->setStyleSheet(
        QStringLiteral("QWidget#control{ background-color: #%1; }")
            .arg(controlBackground.rgba(), 0, 16));
    control->setBackgroundRole(QPalette::ColorRole::AlternateBase);
    control->hide();
    control->layout()->setContentsMargins(0, 0, 0, 0);

    auto controlLeft = new QWidget(control);
    controlLeft->setLayout(new QHBoxLayout(controlLeft));
    controlLeft->layout()->setAlignment(Qt::AlignLeft);
    controlLeft->setSizePolicy(QSizePolicy::Expanding,
                               controlLeft->sizePolicy().horizontalPolicy());
    auto controlRight = new QWidget(control);
    controlRight->setLayout(new QHBoxLayout(controlRight));
    controlRight->layout()->setAlignment(Qt::AlignRight);
    controlRight->setSizePolicy(QSizePolicy::Expanding,
                                controlLeft->sizePolicy().verticalPolicy());

    control->layout()->addWidget(controlLeft);
    control->layout()->addWidget(controlRight);

    if (manifest.launch) {
      if (manifest.launch->protocol.empty()) {
        auto launchBtn = new QToolButton(control);
        launchBtn->setPopupMode(
            QToolButton::ToolButtonPopupMode::MenuButtonPopup);
        auto launchAct = new QAction("Launch", launchBtn);
        auto launchWithAct = new QAction("Launch with ...", launchBtn);
        launchBtn->addAction(launchAct);
        launchBtn->addAction(launchWithAct);
        launchBtn->setIcon(QIcon("icons/run.svg"));
        controlLeft->layout()->addWidget(launchBtn);

        auto launchCb = [context = &context, this] {
          context->callMethodShowErrors("alternative/launch", {},
                                        {{"id", alternative->manifest().id()}});
        };

        connect(launchBtn, &QToolButton::clicked, launchCb);
        connect(launchAct, &QAction::triggered, launchCb);
        connect(launchWithAct, &QAction::triggered, [context = &context, this] {
          context->callMethodShowErrors("alternative/launchWith", {},
                                        {{"id", alternative->manifest().id()}});
        });
      }
    } else if (manifest.install) {
      auto installBtn = new QToolButton(control);
      // TODO
      // installBtn->setPopupMode(QToolButton::ToolButtonPopupMode::MenuButtonPopup);
      // auto installAct = new QAction("Launch", installBtn);
      // auto installToAct = new QAction("Install to ...", installBtn);
      // installBtn->addAction(installAct);
      // installBtn->addAction(installToAct);
      installBtn->setIcon(QIcon("icons/install.svg"));
      controlLeft->layout()->addWidget(installBtn);

      connect(installBtn, &QToolButton::clicked, [context = &context, this] {
        context->callMethodShowErrors("alternative/install", {},
                                      {{"id", alternative->manifest().id()}});
      });
    } else if (manifest.download) {
      auto downloadBtn = new QToolButton(control);
      // TODO
      // downloadBtn->setPopupMode(QToolButton::ToolButtonPopupMode::MenuButtonPopup);
      // auto downloadAct = new QAction("Download", downloadBtn);
      // auto downloadToAct = new QAction("Download to ...", downloadBtn);
      // downloadBtn->addAction(downloadAct);
      // downloadBtn->addAction(downloadToAct);
      downloadBtn->setIcon(QIcon("icons/download.svg"));
      controlLeft->layout()->addWidget(downloadBtn);

      connect(downloadBtn, &QToolButton::clicked, [context = &context, this] {
        context->callMethodShowErrors("alternative/download", {},
                                      {{"id", alternative->manifest().id()}},
                                      [](auto) {});
      });
    }

    if (manifest.launch) {
      auto configBtn = new QToolButton(control);
      configBtn->setIcon(QIcon("icons/gear.svg"));
      controlRight->layout()->addWidget(configBtn);

      connect(
          configBtn, &QToolButton::clicked,
          [configBtn, context = &context, this] {
            QMenu menu;
            if (auto settingsSchema = alternative->findUiSchema("settings")) {
              auto configAct = new QAction("Settings", &menu);
              menu.addAction(configAct);

              connect(configAct, &QAction::triggered, this, [=, this] {
                auto widget = createWidget(
                    settingsSchema,
                    context->getSettingsFor(alternative, "config/settings"));
                widget->setAttribute(Qt::WA_DeleteOnClose);
                widget->show();
              });
            }
            auto deleteAct = new QAction("Delete", &menu);
            menu.addAction(deleteAct);
            auto pos = configBtn->mapToGlobal(QPoint());
            pos.setY(pos.y() + configBtn->height());
            menu.exec(pos);
          });
    }

    layout()->addWidget(iconSvg);
    layout()->addWidget(icon);
    layout()->addWidget(label);
  }

  ~AlternativeViewWidget() {
    if (initIconFuture.isValid()) {
      initIconFuture.cancel();
      initIconFuture.waitForFinished();
    }
  }

  void paintEvent(QPaintEvent *event) override {
    if (!initialized) {
      initialized = true;
      if (alternative->manifest().icon.empty()) {
        QGroupBox::paintEvent(event);
        return;
      }

      auto url = Url::makeFromRelative(Url(alternative->manifest().path),
                                       alternative->manifest().icon);

      initIconFuture = url.asyncGet().then(
          QtFuture::Launch::Async,
          [this, scaleSize = iconSvg->size(), url](QByteArray &&array) {
            if (array.isEmpty()) {
              return;
            }

            if (url.underlying().path().toCaseFolded().endsWith(".svg")) {
              QMetaObject::invokeMethod(this, [this, array = std::move(array)] {
                icon->hide();
                iconSvg->show();
                iconSvg->load(array);
              });

            } else {
              pixmap.loadFromData(array);

              if (pixmap.isNull()) {
                return;
              }

              auto scaledPixmap =
                  pixmap.scaled(scaleSize.width(), scaleSize.height(),
                                Qt::KeepAspectRatio, Qt::SmoothTransformation);

              QMetaObject::invokeMethod(
                  this, [this, scaledPixmap = std::move(scaledPixmap)] {
                    icon->show();
                    iconSvg->hide();
                    icon->clear();
                    icon->setPixmap(scaledPixmap);
                  });
            }
          });
    }

    QGroupBox::paintEvent(event);
  }

  bool event(QEvent *event) override {
    if (event->type() == QEvent::HoverEnter) {
      control->setGeometry(QRect(0, 0, width(), 50));
      control->show();
      return true;
    }
    if (event->type() == QEvent::HoverLeave) {
      control->hide();
      return true;
    }
    return QGroupBox::event(event);
  }
};

class IconListViewWidget final : public QWidget {
  Context *context;
  QWidget *searchGroup;
  QWidget *iconsGroup;
  QLineEdit *searchText;
  std::vector<AlternativeViewWidget *> icons;

public:
  IconListViewWidget(Context &context, QWidget *parent = nullptr)
      : QWidget(parent), context(&context) {
    setLayout(new QVBoxLayout(this));
    layout()->setContentsMargins(0, 0, 0, 0);
    auto scrollArea = new QScrollArea(this);

    searchGroup = new QWidget(this);
    searchGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    searchGroup->setLayout(new QHBoxLayout(searchGroup));
    searchGroup->layout()->setAlignment(Qt::AlignCenter);
    searchGroup->hide();

    searchText = new QLineEdit(this);
    searchText->setMaximumWidth(300);
    QAction *clearAction = searchText->addAction(QIcon("icons/trash.svg"),
                                                 QLineEdit::TrailingPosition);
    clearAction->setShortcut(QKeySequence::Cancel);

    searchGroup->layout()->addWidget(searchText);
    layout()->addWidget(searchGroup);

    setFocusProxy(scrollArea);
    iconsGroup = new QWidget(this);
    iconsGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setWidget(iconsGroup);
    scrollArea->setWidgetResizable(true);
    scrollArea->installEventFilter(this);
    iconsGroup->setLayout(new FlowLayout(iconsGroup));

    connect(clearAction, &QAction::triggered, [this] {
      searchText->clear();
      searchGroup->hide();

      for (auto &icon : icons) {
        icon->show();
      }
    });

    connect(searchText, &QLineEdit::textChanged, [this] {
      // TODO: implement fuzzy matching
      auto pattern = searchText->text().toCaseFolded();
      for (auto &icon : icons) {
        if (icon->label->text().toCaseFolded().contains(pattern)) {
          icon->show();
        } else {
          icon->hide();
        }
      }
    });

    layout()->addWidget(scrollArea);
  }

  void addItem(std::shared_ptr<Alternative> item) {
    auto iconView =
        new AlternativeViewWidget(*context, std::move(item), iconsGroup);
    iconsGroup->layout()->addWidget(iconView);
    icons.push_back(iconView);
  }

  void removeItem(std::string_view id) {
    for (auto it = icons.begin(); it != icons.end(); ++it) {
      if ((*it)->alternative->manifest().id() == id) {
        auto iconView = *it;
        icons.erase(it);
        iconsGroup->layout()->removeWidget(iconView);
        delete iconView;
        return;
      }
    }
  }

  bool eventFilter(QObject *watched, QEvent *event) override {
    if (event->type() == QEvent::KeyPress) {
      auto kEvent = static_cast<QKeyEvent *>(event);
      auto isCtrlF = kEvent->modifiers() == Qt::ControlModifier &&
                     kEvent->key() == Qt::Key_F;
      auto isCharacter = kEvent->key() > 0x20 && kEvent->key() < 0x80 &&
                         (kEvent->modifiers() == Qt::NoModifier ||
                          kEvent->modifiers() == Qt::ShiftModifier);

      if (isCtrlF || isCharacter) {
        if (searchGroup->isHidden()) {
          searchGroup->show();
        }

        if (!searchText->hasFocus()) {
          searchText->setFocus();
          return searchText->event(event);
        }
      }
    }
    return QWidget::eventFilter(watched, event);
  }
};

static QWidget *
createPackagesWidget(Context &context,
                     const AlternativeRequirements &requirements,
                     QWidget *topWidget) {
  auto widget = new QWidget();
  widget->resize(800, 600);
  widget->setLayout(new QVBoxLayout(widget));

  if (topWidget != nullptr) {
    widget->layout()->addWidget(topWidget);
  }
  auto iconList = new IconListViewWidget(context, widget);
  widget->layout()->addWidget(iconList);
  widget->setFocusProxy(iconList);
  iconList->setFocus();

  auto watchConnection = context.createNotificationHandler(
      "packages/change", [=, context = &context](NotificationArgs args) {
        if (args.contains("add")) {
          for (auto &id : args["add"]) {
            auto alt = context->findAlternativeById(id.get<std::string>());
            if (alt == nullptr) {
              continue;
            }

            if (alt->manifest().source.empty()) {
              // ignore builtins and groups
              continue;
            }

            if (!alt->manifest().install && !alt->manifest().download &&
                !alt->manifest().launch) {
              // no actions
              continue;
            }

            if (!alt->match(requirements)) {
              continue;
            }

            QMetaObject::invokeMethod(iconList,
                                      [iconList, alt = std::move(alt)] {
                                        iconList->addItem(std::move(alt));
                                      });
          }
        }

        if (args.contains("remove")) {
          for (auto &id : args["remove"]) {
            QMetaObject::invokeMethod(iconList,
                                      [iconList, id = id.get<std::string>()] {
                                        iconList->removeItem(id);
                                      });
          }
        }
      });

  for (auto &alt : context.allAlternatives) {
    if (alt.second->manifest().source.empty()) {
      // ignore builtins and groups
      continue;
    }

    if (!alt.second->manifest().install && !alt.second->manifest().download &&
        !alt.second->manifest().launch) {
      // no actions
      continue;
    }

    if (!alt.second->match(requirements)) {
      continue;
    }

    iconList->addItem(alt.second);
  }

  QObject::connect(widget, &QWidget::destroyed,
                   [watchConnection = std::move(watchConnection)] mutable {
                     watchConnection.destroy();
                   });

  return widget;
}

static void showPackagesDialog(
    Context &context, const MethodCallArgs &args,
    std::move_only_function<void(MethodCallResult)> responseHandler) {
  AlternativeRequirements requirements;

  if (args.contains("requirements")) {
    requirements = args["requirements"].get<AlternativeRequirements>();
  }

  auto menuBar = new QMenuBar();
  auto menu = new QMenu(menuBar);
  menuBar->addMenu(menu);
  menu->setTitle("File");
  QObject::connect(
      menu->addAction("Sources"), &QAction::triggered,
      [context = &context] { context->showView("package-sources"); });

  auto widget = createPackagesWidget(context, requirements, menuBar);
  widget->setWindowTitle(QString::fromStdString("Packages"));
  menuBar->setParent(widget);

  if (args.contains("dialog") && args["dialog"]) {
    auto dialog = new QDialog();
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    widget->setParent(dialog);
    dialog->setLayout(new QVBoxLayout(dialog));
    dialog->setContentsMargins(0, 0, 0, 0);
    dialog->layout()->addWidget(widget);
    dialog->setModal(true);
    dialog->exec();
    responseHandler({});
  } else {
    widget->setAttribute(Qt::WA_DeleteOnClose);
    widget->show();
    widget->setFocus();
    responseHandler({});
  }
};

static QWidget *createMainWidget(Context &context) {
  auto menuBar = new QMenuBar();
  auto menu = new QMenu(menuBar);
  menuBar->addMenu(menu);
  menu->setTitle("File");
  QObject::connect(menu->addAction("Packages"), &QAction::triggered,
                   [context = &context] { context->showView("packages"); });
  QObject::connect(menu->addAction("Devices"), &QAction::triggered,
                   [context = &context] { context->showView("devices"); });

  auto widget = createPackagesWidget(context, {.hasLaunch = true}, menuBar);
  menuBar->setParent(widget);

  widget->setWindowTitle(QString::fromStdString("ELP Launcher"));
  widget->resize(800, 600);
  widget->hide();

  return widget;
}

struct BuiltinAlternatives final : public Alternative {
  QWidget *mainWidget;
  BuiltinAlternatives(Context &context)
      : Alternative(
            Manifest{.name = "Built-in alternatives",
                     .contributes{.views = {"main", "package-sources",
                                            "alternative-resolver", "packages",
                                            "devices", "error"}}}) {}

  std::error_code activate(Context &context) override {
    mainWidget = createMainWidget(context);

    return {};
  }

  std::error_code deactivate(Context &context) override {
    delete mainWidget;

    return {};
  }

  void callMethod(Context &context, std::string_view name, MethodCallArgs args,
                  std::move_only_function<void(MethodCallResult)>
                      responseHandler) override {
    if (name == "view/show") {
      auto id = args["id"].get<std::string>();

      if (id == "main") {
        mainWidget->show();
        mainWidget->setFocus();
        responseHandler({});
        return;
      }

      if (id == "package-sources") {
        showPackageSourcesDialog(context, args["params"],
                                 std::move(responseHandler));
        return;
      }

      if (id == "packages") {
        showPackagesDialog(context, args["params"], std::move(responseHandler));
        return;
      }

      if (id == "alternative-resolver") {
        showAlternativeResolverDialog(context, args["params"],
                                      std::move(responseHandler));
        return;
      }

      if (id == "devices") {
        // FIXME: implement
        responseHandler({});
        return;
      }

      if (id == "error") {
        showErrorDialog(context, args["params"], std::move(responseHandler));
        return;
      }

      responseHandler({{"error", elp::ErrorCode::InvalidParam}});
      return;
    }

    if (name == "view/hide") {
      auto id = args["id"].get<std::string>();
      if (id == "main") {
        mainWidget->hide();
      }
      responseHandler({});
      return;
    }

    responseHandler({{"error", elp::ErrorCode::MethodNotFound}});
  }
};

struct BuiltinMethodHandler : Alternative {
  std::map<std::string,
           std::move_only_function<MethodCallResult(MethodCallArgs)>,
           std::less<>>
      methodHandlers;

  BuiltinMethodHandler() : Alternative({.name = "Built-in method handler"}) {}

  void callMethod(Context &context, std::string_view name, MethodCallArgs args,
                  std::move_only_function<void(MethodCallResult)>
                      responseHandler) override {
    if (auto it = methodHandlers.find(name); it != methodHandlers.end()) {
      responseHandler(it->second(std::move(args)));
      return;
    }

    responseHandler({{"error", elp::ErrorCode::MethodNotFound}});
  }

  void addHandler(
      std::string name,
      std::move_only_function<MethodCallResult(MethodCallArgs)> handler) {
    methodHandlers[std::move(name)] = std::move(handler);
  }

  static std::shared_ptr<BuiltinMethodHandler> instance() {
    static auto result = std::make_shared<BuiltinMethodHandler>();
    return result;
  }
};

struct BuiltinMethods final : Alternative {
  std::map<std::string,
           std::move_only_function<MethodCallResult(MethodCallArgs)>,
           std::less<>>
      methods;
  BuiltinMethods()
      : Alternative(Manifest{
            .name = "Built-in methods",
            .contributes =
                {
                    .methods =
                        {
                            "alternative/launch",
                            "alternative/launchWith",
                            "alternative/download",
                            "alternative/install",
                            "alternative/delete",
                            "view/show",
                            "view/hide",
                        },
                },
        }) {}

  void callMethod(Context &context, std::string_view name, MethodCallArgs args,
                  std::move_only_function<void(MethodCallResult)>
                      responseHandler) override {
    if (auto it = methods.find(name); it != methods.end()) {
      responseHandler(it->second(std::move(args)));
    } else {
      responseHandler({{"error", elp::ErrorCode::MethodNotFound}});
    }
  }

  void setMethodHandler(
      std::string name,
      std::move_only_function<MethodCallResult(MethodCallArgs)> handler) {
    methods[std::move(name)] = std::move(handler);
  }
};

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  Context context;

  QThreadPool::globalInstance()->setMaxThreadCount(
      std::thread::hardware_concurrency() + 2);

  context.addAlternativeGroup("view/main", "Main widget");
  context.addAlternativeGroup("view/devices", "Devices widget");
  context.addAlternativeGroup("view/error", "Show error widget");
  context.addAlternativeGroup("view/alternative-resolver", "Packages widget");
  context.addAlternativeGroup("view/packages", "Alternative downloader widget");
  context.addAlternativeGroup("view/package-sources",
                              "Packages sources widget");

  context.addAlternativeGroup("linux", "Linux platform");
  context.addAlternativeGroup("windows", "Windows platform");
  context.addAlternativeGroup("ps4", "PlayStation 4 platform");
  context.addAlternativeGroup("ps4-pro", "PlayStation 4 Pro platform");
  context.addAlternativeGroup("ps5", "PlayStation 5 platform");
  context.addAlternativeGroup("hid/ds3", "DualShock 3 platform");
  context.addAlternativeGroup("hid/ds4", "DualShock 4 platform");
  context.addAlternativeGroup("hid/ds", "DualSense platform");

  auto builtinMethodHandlers = std::make_shared<BuiltinMethods>();

  builtinMethodHandlers->setMethodHandler(
      "alternative/launch",
      [&](const MethodCallArgs &args) -> MethodCallResult { return {}; });
  builtinMethodHandlers->setMethodHandler(
      "alternative/launchWith",
      [&](const MethodCallArgs &args) -> MethodCallResult { return {}; });

  builtinMethodHandlers->setMethodHandler(
      "alternative/download",
      [&](const MethodCallArgs &args) -> MethodCallResult { return {}; });

  builtinMethodHandlers->setMethodHandler(
      "alternative/install",
      [&](const MethodCallArgs &args) -> MethodCallResult {
        context.installPackage(args.at("id").get<std::string>());
        return {};
      });
  builtinMethodHandlers->setMethodHandler(
      "alternative/delete",
      [&](const MethodCallArgs &args) -> MethodCallResult { return {}; });

  builtinMethodHandlers->setMethodHandler(
      "window/show", [&](const MethodCallArgs &args) -> MethodCallResult {
        auto errorCode = context.showView(args.at("id").get<std::string>());
        if (errorCode) {
          return {{"error", errorCode.message()}};
        }
        return {{"result", nlohmann::json::object_t{}}};
      });
  builtinMethodHandlers->setMethodHandler(
      "window/hide", [&](const MethodCallArgs &args) -> MethodCallResult {
        auto errorCode = context.hideView(args.at("id").get<std::string>());
        return {{"result", errorCode ? MethodCallResult(errorCode.message())
                                     : MethodCallResult{}}};
      });

  context.addAlternative(std::move(builtinMethodHandlers));

  auto nativeLaunch = QSysInfo::kernelType().toStdString();
  // std::printf("native launcher: %s\n", nativeLaunch.c_str());

  context.addAlternative(std::make_shared<NativeLauncher>(Manifest{
      .name = "native-launcher",
      .contributes =
          {
              .alternatives =
                  {
                      nativeLaunch,
                  },
              .methods =
                  {
                      "launch",
                      "terminate",
                  },
          },
  }));

  context.addAlternative(std::make_shared<BuiltinAlternatives>(context));

  {
    auto appConfigLocation = QStandardPaths::writableLocation(
        QStandardPaths::StandardLocation::AppConfigLocation);
    auto appLocalDataLocation = QStandardPaths::writableLocation(
        QStandardPaths::StandardLocation::AppLocalDataLocation);
    auto configPath = std::filesystem::path(appConfigLocation.toStdString());
    auto dataPath = std::filesystem::path(appLocalDataLocation.toStdString());
    std::filesystem::create_directories(configPath);
    std::filesystem::create_directories(dataPath);

    context.configPath = std::move(configPath);
    context.dataPath = std::move(dataPath);
  }

  context.loadSettings();
  if (context.getSettings("package-sources", Settings::array()).empty()) {
    context.showView("package-sources", {});
  }

  if (context.showView("main")) {
    std::fprintf(stderr, "Failed to open main window\n");
    return 1;
  }

  context.updatePackageSources();

  app.exec();
  context.saveSettings();
  return 0;
}
