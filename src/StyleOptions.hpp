#pragma once

#include <functional>
#include <map>
#include <qobject.h>
#include <string>
#include <vector>

namespace detail {
template <typename... T> struct ArgTypeTest {
  template <typename U>
    requires(std::is_same_v<std::remove_cvref_t<U>, T> || ...)
  consteval operator U() const;
};
} // namespace detail

template <typename ObjectT> struct StyleOptionHandlerList {
  std::map<std::string,
           std::vector<std::function<void(ObjectT *, const QString &)>>,
           std::less<>>
      handlers;

  template <typename T> void add(std::string key, T &&cb) {
    auto handler = [key, cb = std::forward<T>(cb)](ObjectT *obj,
                                                   const QString &value) {
      bool ok = false;
      if constexpr (requires {
                      cb(obj, detail::ArgTypeTest<std::int64_t>());
                    }) {
        auto convValue = value.toLongLong(&ok);
        if (ok) {
          cb(obj, convValue);
          return;
        }
      } else if constexpr (requires {
                             cb(obj, detail::ArgTypeTest<double>());
                           }) {
        auto convValue = value.toDouble(&ok);
        if (ok) {
          cb(obj, convValue);
          return;
        }
      } else if constexpr (requires {
                             cb(obj, detail::ArgTypeTest<std::string,
                                                         std::string_view>());
                           }) {
        cb(obj, value.toStdString());
        return;
      } else if constexpr (requires { cb(obj, detail::ArgTypeTest<bool>()); }) {
        cb(obj, value == "true");
        return;
      } else {
        static_assert(false, "wrong argument type");
      }

      std::fprintf(stderr, "failed to parse value '%s' for '%s'\n",
                   value.toStdString().data(), key.c_str());
    };

    handlers[std::move(key)].push_back(std::move(handler));
  }
};

template <typename LT, typename RT>
  requires requires(LT *l, RT *r) { l = r; }
inline StyleOptionHandlerList<RT> operator|(const StyleOptionHandlerList<LT> &lhs,
                                        StyleOptionHandlerList<RT> rhs) {
  for (auto &l : lhs.handlers) {
    auto &rhsHandlers = rhs.handlers[l.first];
    rhsHandlers.reserve(rhsHandlers.size() + l.second.size());

    for (auto handler : l.second) {
      rhsHandlers.push_back(std::move(handler));
    }
  }

  return std::move(rhs);
}

class StyleOptions {
public:
  using Storage = std::map<std::string, QString, std::less<>>;

  StyleOptions() = default;
  StyleOptions(Storage storage) : m_storage(std::move(storage)) {}

  template <typename T, typename U = T>
  U *visit(const StyleOptionHandlerList<T> &list, U *object) const {
    auto it = m_storage.begin();
    auto handlerIt = list.handlers.begin();

    while (it != m_storage.end() && handlerIt != list.handlers.end()) {
      auto cmpResult = it->first <=> handlerIt->first;

      if (cmpResult < 0) {
        std::fprintf(stderr, "ignored option '%s'\n", it->first.c_str());
        ++it;
        continue;
      }
      if (cmpResult > 0) {
        handlerIt = list.handlers.lower_bound(it->first);
        continue;
      }

      for (auto &&handler : handlerIt->second) {
        handler(object, it->second);
      }
      ++it;
      ++handlerIt;
    }

    for (; it != m_storage.end(); ++it) {
      std::fprintf(stderr, "ignored option '%s'\n", it->first.c_str());
    }

    return object;
  }

private:
  Storage m_storage;
};
