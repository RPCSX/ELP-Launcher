#pragma once

#include "Context.hpp"

struct SchemaNode;
class QWidget;

QWidget *createWidget(SchemaNode *node, Settings &settings);
