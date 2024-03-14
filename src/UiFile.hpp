#pragma once
#include "StyleOptions.hpp"
#include <QXmlStreamReader>

struct SchemaNode {
  virtual ~SchemaNode() = default;

  std::string id;
  std::string type;
  std::string def;
  std::string valueMethod;
  std::string text;
  std::string title;
  bool required = false;
  bool supportsHotChange = false;
  std::string styleHint;
  StyleOptions styleOptions;
  std::vector<std::unique_ptr<SchemaNode>> children;
};

using IdToSchemaMap = std::map<std::string, SchemaNode *, std::less<>>;

std::unique_ptr<SchemaNode> parseUiFile(IdToSchemaMap &idMap, QByteArray xml);
