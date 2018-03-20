#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QThread>
#include <QProcess>
#include <QElapsedTimer>
#include <QTextCursor>
#include <QFontMetrics>
#include "source/data.h"
#include "source/util/util.h"
#include <QWheelEvent>
#include "dialognewimage.h"
#include "source/limage/limageio.h"

#include "source/errorhandler.h"
#include "source/parser.h"
#include "source/interpreter.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
   // m_work.m_colorList.CreateUI(ui->layoutColors,0);
    m_toolBox.Initialize(ui->lyToolbox_3);;
    UpdatePalette();
    m_grid.Initialize(320*2,200*2);
    m_grid.CreateGrid(40,25,m_gridColor,4, 1, QPoint(0,0));


    m_grid.ApplyToLabel(ui->lblGrid);


    //m_updateThread = new QThread(this);
//    m_updateThread->mw = this;
    //QObject::connect(m_updateThread, SIGNAL(valueChanged()), this, SLOT (Update()));

    m_updateThread = new WorkerThread(&m_work, ui, &m_toolBox);
    connect(m_updateThread, SIGNAL(updateImageSignal()), this, SLOT(updateImage()));

    connect(qApp, SIGNAL(aboutToQuit()), m_updateThread, SLOT(OnQuit()));

    m_updateThread->start();

    ui->centralWidget->setLayout(new QGridLayout());

    m_iniFile.Load(m_iniFileName);
    m_iniFile.setString("project_path", m_iniFile.getString("project_path").replace("\\","/"));
    setupEditor();

    if (m_iniFile.getString("current_file")!="")
        LoadRasFile(m_iniFile.getString("current_file"));




#ifndef USE_LIBTIFF
    ui->btnTiff->setVisible(false);
#endif
//    m_updateThread->join();
//    m_updateThread->
}



MainWindow::~MainWindow()
{
    m_quit = true;
    delete ui;
}


void MainWindow::mousePressEvent(QMouseEvent *e)
{
    if(e->buttons() == Qt::RightButton)
        m_updateThread->m_currentButton = 2;
    if(e->buttons() == Qt::LeftButton) {
        m_updateThread->m_currentButton = 1;
        m_work.m_currentImage->AddUndo();

    }


}

void MainWindow::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_updateThread->m_currentButton==2)
        m_updateThread->m_currentButton = 0;
    else
    m_updateThread->m_currentButton = -1;

}

void MainWindow::wheelEvent(QWheelEvent *event)
{
    float f = event->delta()/100.0f;

    if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
        m_updateThread->m_zoom -=f*0.05;
        m_updateThread->m_zoom = min(m_updateThread->m_zoom, 1.0f);
        m_updateThread->m_zoom = max(m_updateThread->m_zoom, 0.1f);
        float t = 0.0f;
        m_updateThread->m_zoomCenter = (m_updateThread->m_zoomCenter*t + (1-t)*m_updateThread->m_currentPos);//*(2-2*m_zoom);
        Data::data.redrawOutput = true;

        m_grid.CreateGrid(40,25,m_gridColor,4, m_updateThread->m_zoom, m_updateThread->m_zoomCenter);

    }
    else {
        m_toolBox.m_current->m_size +=f;
        m_toolBox.m_current->m_size = max(m_toolBox.m_current->m_size,1.0f);
        m_toolBox.m_current->m_size = min(m_toolBox.m_current->m_size,50.0f);
    }
    Data::data.redrawOutput = true;
    Data::data.forceRedraw = true;

}

void MainWindow::keyPressEvent(QKeyEvent *e)
{
//    qDebug() << (QApplication::keyboardModifiers() & Qt::ShiftModifier) << " + "  << rand()%100;
    if (e->key()==Qt::Key_Shift) {
        m_toolBox.m_current->m_type = 1;
        Data::data.redrawOutput = true;
        Data::data.forceRedraw = true;
    }

    if (!ui->tblData->hasFocus()) {
        m_work.m_currentImage->m_image->StoreData(ui->tblData);
        m_work.m_currentImage->m_image->KeyPress(e);
        m_work.m_currentImage->m_image->BuildData(ui->tblData, m_iniFile.getStringList("data_header"));
    }
    FillCMBColors();

  //  if (e->key()==Qt::Key_F)
    //    m_work.m_currentImage->m_image->Fix();

/*    if (e->key() == Qt::Key_Enter && ui->leSearch->hasFocus()) {
        m_searchFromPos=m_currentFromPos+1;
        SearchInSource();
    }*/

    if (e->key() == Qt::Key_Escape && ui->leSearch->hasFocus()) {
        ui->txtEditor->setFocus();
    }

    if (e->key()==Qt::Key_F && QApplication::keyboardModifiers() & Qt::ControlModifier) {
            ui->leSearch->setText("");
            ui->leSearch->setFocus();
            m_searchFromPos = 0;
    }


    if (e->key()==Qt::Key_Z && QApplication::keyboardModifiers() & Qt::ControlModifier) {
        m_work.m_currentImage->Undo();

    }
    if (e->key() == Qt::Key_Z  && !(QApplication::keyboardModifiers() & Qt::ControlModifier)) {
            ui->chkGrid->setChecked(!ui->chkGrid->isChecked());
            ui->lblGrid->setVisible(ui->chkGrid->isChecked());

    }
    if (e->key() == Qt::Key_X) {
        m_work.m_currentImage->m_image->renderPathGrid =!m_work.m_currentImage->m_image->renderPathGrid;
    }
    if (e->key() == Qt::Key_S &&  (QApplication::keyboardModifiers() & Qt::ControlModifier)) {
        on_btnSave_2_clicked();
    }
    if (e->key() == Qt::Key_B &&  (QApplication::keyboardModifiers() & Qt::ControlModifier)) {
        Build();
    }
    if (e->key() == Qt::Key_R &&  (QApplication::keyboardModifiers() & Qt::ControlModifier)) {
        Build();
        Run();
    }
    Data::data.forceRedraw = true;
    Data::data.Redraw();



}

void MainWindow::keyReleaseEvent(QKeyEvent *e)
{
    if (e->key()==Qt::Key_Shift) {
        m_toolBox.m_current->m_type = 0;
        Data::data.redrawOutput = true;
        Data::data.forceRedraw = true;
    }

}

void MainWindow::UpdatePalette()
{
    if (m_work.m_currentImage==nullptr)
        return;
    LColorList* l = &m_work.m_currentImage->m_image->m_colorList;
    //if (m_currentColorList!=l)
    //{

        l->CreateUI(ui->layoutColorsEdit_3,1);
        l->FillComboBox(ui->cmbBackgroundMain_3);
        l->FillComboBox(ui->cmbBorderMain_3);
        l->FillComboBox(ui->cmbMC1);
        l->FillComboBox(ui->cmbMC2);
        m_currentColorList = l;
    //}

    if (m_work.m_currentImage==nullptr)
        return;

    if (m_work.m_currentImage->m_image==nullptr)
        return;


    ui->cmbMC1->setCurrentIndex(m_work.m_currentImage->m_image->m_extraCols[1]);
    ui->cmbMC2->setCurrentIndex(m_work.m_currentImage->m_image->m_extraCols[2]);

    ui->btnExportBin->setVisible(m_work.m_currentImage->m_image->m_supports.binarySave);
    ui->btnImportBin->setVisible(m_work.m_currentImage->m_image->m_supports.binaryLoad);
    ui->btnLoad->setVisible(m_work.m_currentImage->m_image->m_supports.flfLoad);
    ui->btnSave->setVisible(m_work.m_currentImage->m_image->m_supports.flfSave);
    ui->btnExportAsm->setVisible(m_work.m_currentImage->m_image->m_supports.asmExport);

}

void MainWindow::LoadRasFile(QString fileName)
{
    m_currentSourceFile = m_iniFile.getString("project_path") + "/" + fileName;

    QFile file(m_currentSourceFile);
    if (file.open(QFile::ReadOnly | QFile::Text))
        ui->txtEditor->setPlainText(file.readAll());

    m_iniFile.setString("current_file", fileName);
    m_buildSuccess = false;
    SetLights();

}

void MainWindow::ExecutePrg(QString fileName)
{
    QProcess process;
    process.waitForFinished();
    process.startDetached(m_iniFile.getString("emulator"), QStringList() << fileName);
    QString output(process.readAllStandardOutput());
    qDebug() << output;

}

void MainWindow::SetLights()
{
    if (!m_buildSuccess)
        ui->lblLight->setStyleSheet("QLabel { background-color : red; color : blue; }");
    else
        ui->lblLight->setStyleSheet("QLabel { background-color : green; color : blue; }");
}


void MainWindow::on_actionImport_triggered()
{

}

void MainWindow::on_btnConvert_clicked()
{
    //m_work.Convert();
    //m_updateThread->UpdateOutput();

}
void MainWindow::Update()
{

}

void MainWindow::FillCMBColors()
{
//    qDebug() << "Extracols 1: " << QString::number(m_work.m_currentImage->m_image->m_extraCols[1]);
    ui->cmbBackgroundMain_3->setCurrentIndex(m_work.m_currentImage->m_image->m_background);
    ui->cmbMC1->setCurrentIndex(m_work.m_currentImage->m_image->m_extraCols[1]);
    ui->cmbMC2->setCurrentIndex(m_work.m_currentImage->m_image->m_extraCols[2]);
}

void MainWindow::MainWindow::setupEditor()
{
//    return;
    QFont font;
    font.setFamily("Courier");
    font.setFixedPitch(true);
    font.setPointSize(12);


    // Set up file system model
    RefreshFileList();
    ui->treeFiles->hideColumn(1);
    ui->treeFiles->hideColumn(2);
    ui->treeFiles->hideColumn(3);

//    ui->txtEditor = &m_codeEditor;

    ui->txtEditor->setFont(font);
    //ui->txtEditor->setTextColor(QColor(220,210,190));
    highlighter = new Highlighter(ui->txtEditor->document());

    QFontMetrics metrics(font);
    ui->txtEditor->setTabStopWidth(m_iniFile.getInt("tab_size") * metrics.width(' '));


}

void MainWindow::Build()
{
    if (!m_currentSourceFile.toLower().endsWith(".ras")) {
        ui->txtOutput->setText("Turbo Rascal SE can only compile .ras files");
        m_buildSuccess = false;
        return;
    }


    QString text = ui->txtEditor->toPlainText();
    ErrorHandler::e.m_level = ErrorHandler::e.ERROR_ONLY;
    ErrorHandler::e.m_teOut = "";
    ErrorHandler::e.exitOnError = false;
    QStringList lst = text.split("\n");
 //   text = text.replace("\n","");
//    SymbolTable::isInitialized = true;

    QElapsedTimer timer;
    timer.start();
    Lexer lexer = Lexer(text, lst, m_iniFile.getString("project_path"));
    Parser parser = Parser(&lexer);
    Interpreter interpreter = Interpreter(&parser);
    interpreter.Parse();
//    interpreter.Interpret();

    //interpreter.Build(Interpreter::PASCAL);
    //interpreter.SaveBuild(m_outputFilename+".pmm");

    QString path = m_iniFile.getString("project_path") + "/";
    QString filename = m_currentSourceFile.split(".")[0];

    if (interpreter.Build(Interpreter::MOS6502, path)) {
        interpreter.SaveBuild(filename + ".asm");
        QString text ="Build <b><font color=\"#90FF90\">Successful</font>!</b><br>";
        text+="Assembler file saved to : <b>" + filename+".asm</b><br>";
        text+="Compiled " + QString::number(parser.m_lexer->m_lines.count()) +" of Rascal to ";
        text+=QString::number(interpreter.m_assembler->m_source.count()) + " lines of DASM assembler<br>";
        text+="Time: " + Util::MilisecondToString(timer.elapsed())+"<br>";
        text+="**** DASM output:<br>";
        QProcess process;
        process.start(m_iniFile.getString("dasm"), QStringList()<<(filename +".asm") << ("-o"+filename+".prg"));
        process.waitForFinished();
        QString output(process.readAllStandardOutput());
        QString size = QString::number(QFile(filename+".prg").size());

        output +="<br>Assembled file size: <b>" + size + "</b> bytes";
        ui->txtOutput->setText(text + output);
        if (output.toLower().contains("error"))
            m_buildSuccess = false;
        else

        m_buildSuccess = true;
    }
    else {
        ui->txtOutput->setText(ErrorHandler::e.m_teOut);
        int ln = Pmm::Data::d.lineNumber;
        QTextCursor cursor(ui->txtEditor->document()->findBlockByLineNumber(ln-1));
        ui->txtEditor->setTextCursor(cursor);
        m_buildSuccess = false;

    }
    SetLights();
}


void MainWindow::on_btnExportAsm_clicked()
{

    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("Export Multicolor Assembler image"), m_iniFile.getString("project_path"),
                                                    tr("Bin (*.bin);"));


//    m_work.m_currentImage->m_image->ExportAsm(fileName);
    MultiColorImage* mi = (MultiColorImage*)dynamic_cast<MultiColorImage*>(m_work.m_currentImage->m_image);

    if (mi==nullptr)
        return;

    //fileName.remove(".bin");

    mi->ExportAsm(fileName);
//    mi->ExportRasBin(fileName, "");
}

void MainWindow::on_pushButton_clicked()
{
    /*    m_work.m_currentImage->m_image->CopyFrom(&m_work.m_converter.m_mcImage);
    ui->tabWidget->setCurrentIndex(0);
    m_updateThread->UpdateImage((LImage*)m_work.m_currentImage->m_image);*/

}

void MainWindow::on_btnLoad_clicked()
{
    QString f = "Image Files (*." + LImageIO::m_fileExtension + ")";
    QString filename = QFileDialog::getOpenFileName(this,
        tr("Open Image"), m_iniFile.getString("project_path"), f);
    if (filename=="")
        return;

    LImage* img = LImageIO::Load(filename);

    m_work.New(img, filename);

    img->LoadCharset(m_iniFile.getString("current_charset"));
    updateCharSet();

    Data::data.redrawFileList = true;
    Data::data.Redraw();
    UpdatePalette();
    FillCMBColors();

    m_work.m_currentImage->m_image->BuildData(ui->tblData, m_iniFile.getStringList("data_header"));

//    m_updateThread->UpdateImage(m_work.m_currentImage->m_image);
    //delete img;

}

void MainWindow::on_btnSave_clicked()
{
    QString filename = m_work.m_currentImage->m_fileName;

    if (filename=="")
        filename = QFileDialog::getSaveFileName(this,
                                                tr("Save Image"), m_iniFile.getString("project_path"), ("Image Files (*." + LImageIO::m_fileExtension + ")"));
    if (filename=="")
        return;

    LImageIO::Save(filename,m_work.m_currentImage->m_image);

}

void MainWindow::on_chkGrid_clicked(bool checked)
{
//    if (checked)
        ui->lblGrid->setVisible(checked);
}




void MainWindow::on_btnNew_clicked()
{
    m_work.m_currentImage->m_image->Clear();
    Data::data.Redraw();
}

void MainWindow::on_btnExportImage_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Save Image"), m_iniFile.getString("project_path"), tr("Image Files (*.png *.jpg *.bmp )"));

    if (fileName == "")
        return;

    m_updateThread->m_tmpImage->save(fileName);


}

void MainWindow::on_b_clicked()
{


    DialogNewImage* dNewFile = new DialogNewImage(this);
    dNewFile->Initialize(m_work.getImageTypes());
    dNewFile->setModal(true);
    dNewFile->exec();
    if (dNewFile->retVal!=-1) {
        m_work.New(dNewFile->retVal, dNewFile->m_meta);

    }



    UpdatePalette();

    delete dNewFile;
}

void MainWindow::on_lstImages_clicked(const QModelIndex &index)
{
    m_work.SetImage(index.row());
    Data::data.redrawFileList = true;
    UpdatePalette();
}

void MainWindow::on_btnImport_clicked()
{
    DialogImport* di = new DialogImport(this);
    di->Initialize(m_work.m_currentImage->m_imageType->type, m_work.m_currentImage->m_image->m_colorList.m_type);
    di->exec();
    if (di->m_ok) {
        m_work.m_currentImage->m_image->CopyFrom(di->m_image);
        Data::data.redrawOutput = true;
    }

}

void MainWindow::on_btnTiff_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open Tiled Tiff"), m_iniFile.getString("project_path"), tr("Image Files (*.tif *.tiff )"));

    if (fileName == "")
        return;


    LImageTiff *tif = (LImageTiff*)LImageFactory::Create(LImage::Tiff, LColorList::TIFF);
    tif->Initialize(640,480);
    tif->LoadTiff(fileName);
//    tif->m_type = LImage::Tiff;
    m_work.New(tif, fileName);
    Data::data.redrawFileList = true;
    Data::data.Redraw();
    UpdatePalette();

}

void MainWindow::on_btnSaveAs_clicked()
{

    QString filename = QFileDialog::getSaveFileName(this,
                                                tr("Save Image"), m_iniFile.getString("project_path"), ("Image Files (*." + LImageIO::m_fileExtension + ")"));
    if (filename=="")
        return;
    m_work.m_currentImage->m_fileName  =filename;

    LImageIO::Save(filename,m_work.m_currentImage->m_image);

}

void MainWindow::on_btnBuild_clicked()
{
    Build();
}

void MainWindow::on_btnBuild_2_clicked()
{
    Run();
}

void MainWindow::Run() {
    if (!m_buildSuccess)
        return;
    QString filename = m_currentSourceFile.split(".")[0] + ".prg";

    ExecutePrg(filename);
}

void MainWindow::RefreshFileList()
{
    fileSystemModel = new QFileSystemModel(this);
    QString rootPath= m_iniFile.getString("project_path");
    fileSystemModel->setReadOnly(true);
    fileSystemModel->setRootPath(rootPath);
    fileSystemModel->setFilter(QDir::NoDotAndDotDot |
                            QDir::AllDirs |QDir::AllEntries);
    fileSystemModel->setNameFilters(QStringList() << "*.ras" << "*.asm" << "*.txt" << "*.prg" << "*.inc");
    fileSystemModel->setNameFilterDisables(false);
    ui->treeFiles->setModel(fileSystemModel);
    ui->treeFiles->setRootIndex(fileSystemModel->index(rootPath));

}

void MainWindow::updateCharSet()
{
    CharsetImage* charmap = nullptr;
    C64FullScreenChar* ch = dynamic_cast<C64FullScreenChar*>(m_work.m_currentImage->m_image);
    if (ch!=nullptr)
        charmap = ch->m_charset;

    ImageLevelEditor* le = dynamic_cast<ImageLevelEditor*>(m_work.m_currentImage->m_image);
    if (le!=nullptr)
        charmap = le->m_charset;

    if (charmap == nullptr)
        return;

    QVector<QPixmap> maps;
    charmap->ToQPixMaps(maps);


    int cnt = 0;
    ui->lstCharMap->setViewMode(QListView::IconMode);

    for (QPixmap& p: maps) {
        QListWidgetItem *itm = ui->lstCharMap->item(cnt);
        if (itm == nullptr) {
            itm = new QListWidgetItem(QIcon(p) , nullptr, ui->lstCharMap);
            ui->lstCharMap->addItem(itm);

        }
        QIcon ic(p);

        itm->setIcon(ic);
        itm->setData(Qt::UserRole, cnt);
        cnt++;

    }

    ui->lstCharMap->setIconSize(QSize(16,16));

}

void MainWindow::SetMCColors()
{
    int a = ui->cmbMC1->currentIndex();
    int b = ui->cmbMC2->currentIndex();
    int back = ui->cmbBackgroundMain_3->currentIndex();

    m_work.m_currentImage->m_image->SetColor(back, 0);
    m_work.m_currentImage->m_image->SetColor(a, 1);
    m_work.m_currentImage->m_image->SetColor(b, 2);

    updateCharSet();
}

void MainWindow::UpdateLevels()
{
    ImageLevelEditor* le = dynamic_cast<ImageLevelEditor*>(m_work.m_currentImage->m_image);
    if (le==nullptr)
        return;

    QVector<QPixmap> icons = le->CreateIcons();


    Util::clearLayout(ui->gridLevels);
//    ui->gridLevels->set
    for (int i=0;i<le->m_meta.m_sizex;i++)
        for (int j=0;j<le->m_meta.m_sizey;j++) {
            QPushButton* l = new QPushButton();
            l->setText("");
            QIcon icon(icons[j + i*le->m_meta.m_sizey]);
            l->setIcon(icon);
            //l->setScaledContents(true);
            l->setIconSize(QSize(64,64));
            ui->gridLevels->addWidget(l, j,i);

            QObject::connect( l, &QPushButton::clicked,  [=](){
                m_work.m_currentImage->m_image->StoreData(ui->tblData);
                le->SetLevel(QPoint(i,j));
                m_work.m_currentImage->m_image->BuildData(ui->tblData,m_iniFile.getStringList("data_header"));
                Data::data.Redraw();
            }

            );

        }



}


void MainWindow::on_btnSave_2_clicked()
{
    if (QFile::exists(m_currentSourceFile))
        QFile::remove(m_currentSourceFile);
    QString txt = ui->txtEditor->document()->toPlainText();
    QFile file(m_currentSourceFile);
    if (file.open(QIODevice::ReadWrite))
    {
        QTextStream stream(&file);
        stream << txt;
    }
    file.close();
    m_iniFile.Save(m_iniFileName);
}

void MainWindow::on_treeFiles_doubleClicked(const QModelIndex &index)
{
    // Find file in path.. ugh
    QString path = "";
    QStringList pathSplit = m_iniFile.getString("project_path").toLower().replace("\\", "/").split("/");
    QString test = pathSplit.last();
    if (test=="")
        test = pathSplit[pathSplit.count()-2];

    QModelIndex cur = index.parent();
    int cnt=0;
    while (cur.data().toString().toLower()!=test) {

        path=cur.data().toString() + "/" + path;
        cur = cur.parent();
        if (cnt++>20)
            return;
    }


    // Finally load file!
    QString file = index.data().toString();
    if (file.toLower().endsWith(".ras") || file.toLower().endsWith(".asm")
            || file.toLower().endsWith(".inc")) {


        LoadRasFile(path + file);
    }
    if (file.toLower().endsWith(".prg")) {
        ExecutePrg(m_iniFile.getString("project_path")+"/" + file);
    }

}

void MainWindow::on_leSearch_textChanged()
{
    QString i;
    SearchInSource();
}

void MainWindow::SearchInSource()
{
    m_currentFromPos = ui->txtEditor->document()->toPlainText().toLower().indexOf(ui->leSearch->text().toLower(), m_searchFromPos);
    QTextCursor cursor(ui->txtEditor->document()->findBlock(m_currentFromPos));
    ui->txtEditor->setTextCursor(cursor);
}


void MainWindow::on_leSearch_returnPressed()
{
    m_searchFromPos=m_currentFromPos+1;
    SearchInSource();

}

void MainWindow::on_actionNew_triggered()
{
    // New .RAS file

    QFileDialog dialog;
    dialog.setFileMode(QFileDialog::AnyFile);
    QString f = "Ras Files (*.ras)";
    QString filename = dialog.getSaveFileName(NULL, "Create New File",m_iniFile.getString("project_path"),f);

    if (filename=="")
        return;
    QString orgFile;
    //filename = filename.split("/").last();
    filename = filename.toLower().remove(m_iniFile.getString("project_path").toLower());

    //qDebug() << filename;
    QString fn = m_iniFile.getString("project_path") + filename;
    if (QFile::exists(fn))
        QFile::remove(fn);
    QFile file(fn);
    qDebug() << "creating new file: " + fn;
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream s(&file);
        s<< "program spankme;\n";
        s<< "var  \n";
        s<< "   index: byte; \n";
        s<< "begin\n\n";
        s<< "end.\n";
        qDebug() << "Done writing;";
    }

    file.close();
    LoadRasFile(filename);
    RefreshFileList();
}

void MainWindow::on_btnExpChar_clicked()
{
    if (m_work.m_currentImage->m_imageType->type==LImage::Type::HiresBitmap ||
        m_work.m_currentImage->m_imageType->type==LImage::Type::MultiColorBitmap
            )
    {
        MultiColorImage* img = (MultiColorImage*)m_work.m_currentImage->m_image;
        img->CalculateCharIndices();
        qDebug() <<"Number of chars: " <<img->m_organized.count();
        QString filename = m_iniFile.getString("project_path") + "/testchar.inc";
        img->SaveCharRascal(filename,"test");
        RefreshFileList();
    }


}

void MainWindow::on_leSearch_textChanged(const QString &arg1)
{
    SearchInSource();
}

void MainWindow::on_btnImportBin_clicked()
{
    QString f = "Binary Files ( *.bin )";
    QString filename = QFileDialog::getOpenFileName(this,
        tr("Import binary file"), m_iniFile.getString("project_path"), f);
    if (filename=="")
        return;


    QFile file(filename);
    file.open(QIODevice::ReadOnly);
    m_work.m_currentImage->m_image->ImportBin(file);
    file.close();

    Data::data.redrawFileList = true;
    Data::data.Redraw();
    UpdatePalette();

}

void MainWindow::on_btnExportBin_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("Export binary file"), m_iniFile.getString("project_path"),
                                                    tr("Bin (*.bin);"));


//    m_work.m_currentImage->m_image->ExportAsm(fileName);
//    MultiColorImage* mi = (MultiColorImage*)dynamic_cast<MultiColorImage*>(m_work.m_currentImage->m_image);

/*    if (mi==nullptr)
        return;
*/
    if (QFile::exists(fileName))
        QFile::remove(fileName);
    QFile file(fileName);
    file.open(QIODevice::WriteOnly);
    m_work.m_currentImage->m_image->ExportBin(file);
    file.close();

  //  fileName.remove(".bin");

//    mi->ExportRasBin(fileName, "");

}

void MainWindow::on_tabWidget_2_currentChanged(int index)
{
    if (index==1)
        m_work.m_currentImage->m_image->SetCurrentType(LImage::WriteType::Color);
    if (index==3)
        UpdateLevels();
//    if (index==2)
//        m_work.m_currentImage->m_image->SetCurrentType(LImage::WriteType::Character);
}

void MainWindow::on_btnLoadCharmap_clicked()
{
    C64FullScreenChar* charImage = dynamic_cast<C64FullScreenChar*>(m_work.m_currentImage->m_image);

    ImageLevelEditor* editor = dynamic_cast<ImageLevelEditor*>(m_work.m_currentImage->m_image);

    if (editor==nullptr && charImage==nullptr)
        return;


    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open Character map"), m_iniFile.getString("project_path"), tr("Binary Files (*.bin )"));

    if (fileName == "")
        return;


    editor->LoadCharset(fileName);
    m_iniFile.setString("current_charset", fileName);
    m_iniFile.Save(m_iniFileName);
    updateCharSet();

}

void MainWindow::on_lstCharMap_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    int idx = current->data(Qt::UserRole).toInt();
    m_work.m_currentImage->m_image->SetCurrentType(LImage::WriteType::Character);
    Data::data.currentColor = idx;

}


void MainWindow::on_cmbMC1_activated(int index)
{
    SetMCColors();

}

void MainWindow::on_cmbMC2_activated(int index)
{
    SetMCColors();

}

void MainWindow::on_cmbBackgroundMain_3_activated(int index)
{
    m_work.m_currentImage->m_image->setBackground(index);
    SetMCColors();
    Data::data.redrawOutput = true;

}

void MainWindow::on_btnResizeData_clicked()
{
    ImageLevelEditor* img = dynamic_cast<ImageLevelEditor*>(m_work.m_currentImage->m_image);
    if (img==nullptr)
        return;


    DialogNewImage* dResize = new DialogNewImage(this);
    dResize->Initialize(m_work.getImageTypes());
    dResize->setModal(true);
    dResize->SetResizeMeta(img->m_meta);

    dResize->exec();
    if (dResize->retVal!=-1) {
        img->Resize(dResize->m_meta);

    }



    UpdatePalette();

    delete dResize;


}
