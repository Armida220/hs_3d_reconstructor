﻿#include <QtTest/QtTest>
#include <QApplication>

#include "gui/property_field_asignment_dialog.hpp"

namespace test
{

class TestPropertyFieldAsignmentDialog : public QObject
{
  Q_OBJECT
private slots:
  void Test();
};

void TestPropertyFieldAsignmentDialog::Test()
{
  std::string file_content;
  file_content =
    "4    106.9361500    23.6492520    475.5800    4.44    0.11    124.62\n"
    "5    106.9365600    23.6490120    475.2800    4.18    1.61    115.93\n"
    "6    106.9369900    23.6487750    473.4500    7.04    1.46    120.22\n"
    "7    106.9374500    23.6485940    474.3600    2.80    0.77    115.75\n"
    "8    106.9378900    23.6483710    475.9100    4.16    -3.83    128.41\n"
    "9    106.9383200    23.6481150    476.8400    2.56    2.07    122.31\n"
    "10    106.9387700    23.6478940    476.6400    3.34    0.73    123.88\n"
    "11    106.9391600    23.6476120    476.4100    2.56    -1.67    118.59\n"
    "12    106.9396100    23.6473920    476.1900    3.31    -2.61    120.52\n"
    "13    106.9400300    23.6471420    475.0300    4.05    -1.57    117.82\n"
    "14    106.9404700    23.6469480    473.6700    4.92    2.93    121.38\n"
    "15    106.9409000    23.6466960    476.6300    1.55    -4.46    123.42\n"
    "16    106.9413200    23.6464630    475.6800    1.97    1.62    120.93\n"
    "17    106.9417600    23.6462630    475.0300    2.66    0.38    126.12\n"
    "18    106.9421900    23.6460150    476.2800    4.38    -6.83    129.93\n"
    "19    106.9426300    23.6457860    474.8200    5.83    4.19    118.91\n"
    "20    106.9430500    23.6455380    476.1300    4.25    -3.04    125.84\n"
    "21    106.9424100    23.6447140    476.6000    -5.26    5.32    296.47\n"
    "22    106.9420000    23.6450230    467.4200    -0.12    1.36    298.59\n"
    "23    106.9416000    23.6452770    471.6600    -1.18    2.76    287.78\n"
    "24    106.9411500    23.6455360    472.2900    0.80    0.75    291.04\n"
    "25    106.9407000    23.6457860    472.5000    1.85    -0.33    290.45\n"
    "26    106.9402400    23.6460150    470.9500    3.41    2.09    296.05\n"
    "27    106.9397700    23.6462500    470.7700    5.04    -1.57    298.84\n"
    "28    106.9393200    23.6464860    468.3600    7.39    -5.55    300.58\n"
    "29    106.9388700    23.6467400    468.5500    12.96    -1.87    303.76";
  QStringList field_names;
  field_names.push_back("X");
  field_names.push_back("Y");
  field_names.push_back("Z");
  field_names.push_back("Pitch");
  field_names.push_back("Roll");
  field_names.push_back("Heading");
  hs::recon::gui::PropertyFieldAsignmentDialog dialog(file_content,
                                                      field_names);
  dialog.show();
  qApp->exec();
}

}

QTEST_MAIN(test::TestPropertyFieldAsignmentDialog);
#include "test_property_field_asignment_dialog.moc"
