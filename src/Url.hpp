#pragma once

#include <QByteArray>
#include <QFuture>
#include <QUrl>

#include <filesystem>
#include <qdir.h>
#include <string>
#include <string_view>
#include <type_traits>

class Url {
public:
  Url() = default;
  Url(QUrl url) : m_underlying(std::move(url)) {}
  Url(std::string_view url)
      : m_underlying(
            QUrl::fromUserInput(QString::fromUtf8(url.data(), url.size()))) {}
  static Url makeFromRelative(const Url &base, std::string_view relative) {
    if (base.empty()) {
      return relative;
    }

    if (!QDir::isRelativePath(QString::fromUtf8(relative))) {
      Url result(relative);
      if (!result.underlying().isRelative()) {
        return result;
      }
    }

    auto absoluteUrl = base.underlying();
    absoluteUrl.setPath(QDir::cleanPath(absoluteUrl.path() + "/" + QString::fromUtf8(relative)));
    return Url(absoluteUrl);
  }

  template <typename T>
    requires std::is_same_v<T, std::filesystem::path>
  Url(const T &path)
      : m_underlying(QUrl::fromLocalFile(QString::fromUtf8(path.string()))) {}

  QFuture<QByteArray> asyncGet() const;
  std::string toString() const { return m_underlying.toString().toStdString(); }
  bool isLocalPath() const { return m_underlying.isLocalFile(); }
  std::filesystem::path toLocalPath() const {
    return m_underlying.toLocalFile().toStdString();
  }

  QUrl &underlying() { return m_underlying; }
  const QUrl &underlying() const { return m_underlying; }

  bool valid() const { return m_underlying.isValid(); }
  bool empty() const { return m_underlying.isEmpty(); }

  auto operator<=>(const Url &rhs) const {
    return toString() <=> rhs.toString();
  }
  bool operator==(const Url &rhs) const = default;

private:
  QUrl m_underlying;
};
