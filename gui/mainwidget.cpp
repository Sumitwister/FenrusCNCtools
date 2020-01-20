#include "mainwidget.h"
#include "drawarea.h"
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>


MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent) {
    
  QHBoxLayout *hbox = new QHBoxLayout(this);
  QVBoxLayout *left = new QVBoxLayout();
  QVBoxLayout *right = new QVBoxLayout();

  drawarea = new DrawArea(this);


  hbox->addLayout(left);

  hbox->addWidget(drawarea);  

  hbox->addLayout(right);


  setLayout(hbox);
}