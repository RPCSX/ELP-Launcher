#include "UiFile.hpp"

static std::unique_ptr<SchemaNode>
genSchemaUiNode(std::string styleHint, std::string text,
                std::map<std::string, QString, std::less<>> &properties) {
  auto result = std::make_unique<SchemaNode>();
  if (auto it = properties.find("required"); it != properties.end()) {
    result->required = it->second == "true";
    properties.erase(it);
  }

  if (auto it = properties.find("hot"); it != properties.end()) {
    result->supportsHotChange = it->second == "true";
    properties.erase(it);
  }

  if (auto it = properties.find("type"); it != properties.end()) {
    result->type = it->second.toStdString();
    properties.erase(it);
  }

  if (auto it = properties.find("default"); it != properties.end()) {
    result->def = it->second.toStdString();
  }

  result->styleHint = std::move(styleHint);
  result->valueMethod = std::move(text);
  result->styleOptions = {std::move(properties)};
  return result;
}

static std::unique_ptr<SchemaNode> parseUiTree(IdToSchemaMap &idMap,
                                               QXmlStreamReader &reader) {
  std::unique_ptr<SchemaNode> result;
  std::vector<SchemaNode *> stack;
  bool seenStart = false;

  std::map<std::string, QString, std::less<>> properties;
  std::string name;
  std::string id;
  std::string text;

  auto processNode = [&] {
    auto node = genSchemaUiNode(std::move(name), std::move(text), properties);
    auto parent = stack.empty() ? nullptr : stack.back();
    stack.push_back(node.get());

    if (!id.empty()) {
      node->id = id;
      idMap[std::move(id)] = node.get();
    }

    if (result == nullptr) {
      result = std::move(node);
    } else {
      parent->children.push_back(std::move(node));
    }
  };

  while (!reader.atEnd()) {
    switch (reader.readNext()) {
    case QXmlStreamReader::StartElement: {
      if (seenStart) {
        seenStart = false;
        processNode();
      }

      text = {};
      properties = {};

      for (auto &attr : reader.attributes()) {
        properties[attr.name().toString().toStdString()] =
            attr.value().toString();
      }

      if (auto it = properties.find("id"); it != properties.end()) {
        id = it->second.toStdString();
        properties.erase(it);
      } else {
        id = {};
      }

      name = reader.name().toUtf8().toStdString();
      reader.text();
      seenStart = true;
      break;
    }

    case QXmlStreamReader::Comment:
      text = reader.text().toUtf8();
      break;

    case QXmlStreamReader::EndElement:
      if (seenStart) {
        seenStart = false;
        processNode();
      }

      if (!stack.empty()) {
        stack.pop_back();
      }
      continue;

    default:
      continue;
    }
  }

  return result;
}

std::unique_ptr<SchemaNode> parseUiFile(IdToSchemaMap &idMap, QByteArray xml) {
  QXmlStreamReader xmlReader{xml};
  return parseUiTree(idMap, xmlReader);
}
