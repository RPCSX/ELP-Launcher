#include "Url.hpp"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QtConcurrent>

static QNetworkAccessManager *getNetworkAccessManager() {
  static auto *manager = new QNetworkAccessManager();
  return manager;
}

QFuture<QByteArray> Url::asyncGet() const {
  if (m_underlying.isLocalFile()) {
    return QtConcurrent::run([path=m_underlying.toLocalFile()] {
      QFile file(path);
      file.open(QFile::ReadOnly);
      return file.readAll();
    });
  }

  QPromise<QByteArray> promise;
  auto result = promise.future();
  QNetworkReply *reply =
      getNetworkAccessManager()->get(QNetworkRequest(m_underlying));
  QObject::connect(reply, &QNetworkReply::finished,
                   [reply, promise = std::move(promise)] mutable {
                     if (reply->error() == QNetworkReply::NoError) {
                       promise.addResult(reply->readAll());
                       promise.finish();
                     } else {
                       promise.setException(QException());
                     }
                   });
  return result;
}
