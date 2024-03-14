#include "Widget.hpp"
#include "UiFile.hpp"

#include <QAbstractButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QLayout>
#include <QRadioButton>
#include <QWidget>
#include <QWindow>

#include <string_view>

static int parseAlignment(std::string_view alignment) {
  if (alignment == "center") {
    return Qt::AlignCenter;
  }
  if (alignment == "top-left") {
    return Qt::AlignTop | Qt::AlignLeft;
  }
  if (alignment == "bottom-left") {
    return Qt::AlignBottom | Qt::AlignLeft;
  }
  if (alignment == "top-right") {
    return Qt::AlignTop | Qt::AlignRight;
  }
  if (alignment == "bottom-right") {
    return Qt::AlignBottom | Qt::AlignRight;
  }
  if (alignment == "bottom-center") {
    return Qt::AlignBottom | Qt::AlignHCenter;
  }
  if (alignment == "top-center") {
    return Qt::AlignTop | Qt::AlignHCenter;
  }
  if (alignment == "center-left") {
    return Qt::AlignVCenter | Qt::AlignLeft;
  }
  if (alignment == "center-right") {
    return Qt::AlignVCenter | Qt::AlignRight;
  }

  return Qt::AlignTop | Qt::AlignLeft;
}

static const auto widgetOptionHandlers = [] {
  StyleOptionHandlerList<QWidget> result;
  result.add("width", [](QWidget *widget, std::int64_t value) {
    widget->setFixedWidth(value);
  });
  result.add("height", [](QWidget *widget, std::int64_t value) {
    widget->setFixedHeight(value);
  });
  result.add("minWidth", [](QWidget *widget, std::int64_t value) {
    widget->setMinimumWidth(value);
  });
  result.add("minHeight", [](QWidget *widget, std::int64_t value) {
    widget->setMinimumHeight(value);
  });
  result.add("maxWidth", [](QWidget *widget, std::int64_t value) {
    widget->setMaximumWidth(value);
  });
  result.add("maxHeight", [](QWidget *widget, std::int64_t value) {
    widget->setMaximumHeight(value);
  });
  result.add("title", [](QWidget *widget, std::string_view value) {
    widget->setWindowTitle(
        QUtf8StringView(value.data(), value.size()).toString());
  });
  result.add("description", [](QWidget *widget, std::string_view value) {
    widget->setToolTip(QUtf8StringView(value.data(), value.size()).toString());
  });
  return result;
}();

static const auto groupOptionHandlers = widgetOptionHandlers | [] {
  StyleOptionHandlerList<QGroupBox> result;
  result.add("title", [](QGroupBox *widget, std::string_view value) {
    widget->setTitle(QUtf8StringView(value.data(), value.size()).toString());
  });
  result.add("align", [](QGroupBox *widget, std::string_view value) {
    widget->setAlignment(parseAlignment(value));
  });
  return result;
}();

static const auto abstractButtonOptionHandlers = widgetOptionHandlers | [] {
  StyleOptionHandlerList<QAbstractButton> result;
  result.add("text", [](QAbstractButton *widget, std::string_view value) {
    widget->setText(QUtf8StringView(value.data(), value.size()).toString());
  });
  result.add("default", [](QAbstractButton *widget, bool value) {
    widget->setChecked(value);
  });

  return result;
}();

static QWidget *createWidgetImpl(std::string_view style,
                                 const StyleOptions &styleOptions) {
  if (style == "group" || style == "hgroup") {
    auto result = new QGroupBox();
    result->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    return styleOptions.visit(groupOptionHandlers, result);
  }
  if (style == "check") {
    return styleOptions.visit(abstractButtonOptionHandlers, new QCheckBox());
  }
  if (style == "radio") {
    return styleOptions.visit(abstractButtonOptionHandlers, new QRadioButton());
  }

  if (!style.empty() && style != "window") {
    std::fprintf(stderr, "unknown style '%s'\n", std::string(style).c_str());
  }

  return styleOptions.visit(widgetOptionHandlers, new QWidget());
}

static QWidget *createWidget(QWidget *parent, SchemaNode *node,
                             Settings &settings) {
  auto result = createWidgetImpl(node->styleHint, node->styleOptions);

  if (parent == nullptr || node->styleHint == "group" ||
      node->styleHint == "window") {
    result->setLayout(new QVBoxLayout(result));
  } else if (node->styleHint == "hgroup") {
    result->setLayout(new QHBoxLayout(result));
  }

  if (parent != nullptr) {
    result->setParent(parent);

    if (auto layout = parent->layout()) {
      layout->addWidget(result);
    }
  }

  for (auto &child : node->children) {
    createWidget(result, child.get(), settings);
  }

  if (!node->id.empty()) {
    if (auto widget = qobject_cast<QGroupBox *>(result)) {
      auto [it, inserted] = settings.emplace(node->id, node->def);
      for (std::size_t i = 0; auto child : widget->children()) {
        if (!child->isWidgetType()) {
          continue;
        }

        if (auto btn = qobject_cast<QAbstractButton *>(child)) {
          if (btn->text().toStdString() == it.value().get<std::string>()) {
            btn->setChecked(true);
          }

          QObject::connect(btn, &QAbstractButton::clicked, [=](bool value) {
            if (value) {
              it.value() = btn->text().toStdString();
            }
          });
        }
      }
    } else if (auto widget = qobject_cast<QAbstractButton *>(result)) {
      auto [it, inserted] = settings.emplace(node->id, node->def == "true");
      if (it.value().is_boolean()) {
        widget->setChecked(it.value().get<bool>());
      }

      QObject::connect(widget, &QAbstractButton::toggled,
                       [=](bool value) { it.value() = value; });
    }
  }

  return result;
}

QWidget *createWidget(SchemaNode *node, Settings &settings) {
  if (!settings.is_object()) {
    settings = Settings::object();
  }
  return createWidget(nullptr, node, settings);
}
