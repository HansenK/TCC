#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "background.cpp"

using namespace std;
using namespace cv;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QMainWindow::showMaximized(); //Inicia com a janela maximizada

    /*** IMPORTA O ARQUIVO DE ESTILIZACAO ***/
    QFile File(":/style/style.qss");
    File.open(QFile::ReadOnly);
    QString StyleSheet = QLatin1String(File.readAll());
    qApp->setStyleSheet(StyleSheet);

    /*** CONFIGURACAO INICIAL ***/
    ui->tabWidget->setTabEnabled(1,false);
    ui->tabWidget->setTabEnabled(2,false);
    ui->tabWidget->setTabEnabled(3,false);
    ui->tabWidget->setTabEnabled(4,false);

    ui->btnConfig->setEnabled(false);
    ui->meassureResultBox->setEnabled(false);

    ui->adjustSlider->hide();
    ui->confirmBox->hide();
    ui->slider->hide();
    //ui->distanceBox->hide();

    ui->graphicsViewPlayer->setScene(new QGraphicsScene(this));
    ui->graphicsViewPlayer->scene()->addItem(&pixmapPlayer);

    ui->graphicsView->setScene(new QGraphicsScene(this));
    ui->graphicsView->scene()->addItem(&pixmap);

    ui->graphicsViewBackground->setScene(new QGraphicsScene(this));
    ui->graphicsViewBackground->scene()->addItem(&pixmapBackground);

    ui->viewSelector->addItem("tracking");
    ui->viewSelector->addItem("máscara");
    ui->viewSelector->addItem("movimento");
    ui->viewSelector->addItem("orientação");

    //*** EVENTOS ***//
    ui->subEventParam->hide();
    ui->subEventParamSelect->hide();
    ui->subEventSelect->addItem("Selecione");
    ui->subEventSelect->addItem("Entrada em Zona");
    ui->subEventSelect->addItem("Saída de Zona");

    //*** PROJETOS NO DIRETORIO RAIZ ***//
    QDir rootDir("C:/LABEHAVE");
    QStringList projectsList = rootDir.entryList(QDir::Dirs|QDir::NoDotAndDotDot);
    ui->recentProjects->addItems(projectsList);
}

MainWindow::~MainWindow()
{
    delete ui;
}

/*** CRIAR NOVO PROJETO ***/
void MainWindow::on_actionNewProject_triggered()
{
    projectName = QInputDialog::getText(this, "Nome do Projeto", "Insira o nome do projeto:");

    projectDir.setPath("C:/LABEHAVE/" + projectName);
    if (!projectDir.exists()) {
        projectDir.mkpath(".");
        ui->tabWidget->setTabEnabled(1,true);
        ui->tabWidget->setTabEnabled(2,true);
        ui->toolBox->setEnabled(true);
        ui->projectNameLabel->setText(projectName);
        ui->tabWidget->setCurrentIndex(1);

        // Cria arquivo de configuracoes JSON
        QJsonObject settingsJsonObj{
          {"name", projectName},
          {"dir", projectDir.absolutePath()}
        };

        QJsonDocument settingsJsonDoc(settingsJsonObj);
        QFile settingsFile(projectDir.absolutePath() + "/conf.lbh");
        Q_ASSERT(settingsFile.open(QFile::WriteOnly));

        settingsFile.write(settingsJsonDoc.toJson());
        settingsFile.close();

    }else QMessageBox::information(this, tr("Erro"), tr("O projeto já existe."));

}// Botao
    void MainWindow::on_btnNewProject_pressed(){ ui->actionNewProject->trigger(); }

/*** ABRIR PROJETO ***/
void MainWindow::openProject(QString projectFileName){
    ui->tabWidget->setTabEnabled(1,true);
    ui->tabWidget->setCurrentIndex(1);

    // Le o arquivo de configuracoes JSON
    QFile settingsFile(projectFileName);
    Q_ASSERT(settingsFile.open(QFile::ReadOnly));
    QJsonDocument settingsJsonDoc = QJsonDocument::fromJson(settingsFile.readAll());
    QJsonObject settingsJsonObj = settingsJsonDoc.object();

    projectName = settingsJsonObj["name"].toString();
    projectDir= settingsJsonObj["dir"].toString();
    ui->projectNameLabel->setText(projectName);

    settingsFile.close();
}
bool MainWindow::on_actionOpenProject_triggered()
{
    QString projectFileName = QFileDialog::getOpenFileName(
                                            this,
                                            tr("Abrir Projeto"),
                                            "C://",
                                            "Projetos (*.lbh)"
                                            );
    if(projectFileName.isEmpty()) return false;
    openProject(projectFileName);
    return true;
}
// pressionando o botao
void MainWindow::on_btnLoadProject_pressed(){ ui->actionOpenProject->trigger(); }
// a partir da lista
void MainWindow::on_recentProjects_itemDoubleClicked(QListWidgetItem *item){
    QString projectFileName = "C:/LABEHAVE/" + item->text() + "/conf.lbh";
    openProject(projectFileName);
}

/*** ORGANIZA OS CONTORNOS ***/
void sortContours(vector<vector<Point>>& contours){
    auto contourComparator = [](vector<Point> a, vector<Point> b){ return contourArea(a) > contourArea(b); };
    sort(contours.begin(), contours.end(), contourComparator);
}
Point MainWindow::getFrameCursor(QGraphicsView& graphicsView, Mat& frame){
    QPoint qp = graphicsView.mapFromGlobal(QPoint(QCursor::pos()));
    int    DX = frame.cols,
           dx = (float)graphicsView.width(),
           DY = frame.rows,
           dy = (float)graphicsView.height();
    float  pmx = (float)DX/(float)dx,
           pmy = (float)DY/(float)dy;

    if(DY>DX) pmx *= ((float)DY*(float)dx)/((float)DX*(float)dy);
    else      pmy /= ((float)DY*(float)dx)/((float)DX*(float)dy);

    Point p = Point( qp.x()*pmx + ((float)DX/2 - (float)dx*pmx/2) ,
                     qp.y()*pmy + ((float)DY/2 - (float)dy*pmy/2) );

    if      (p.x > DX) p.x = DX;
    else if (p.x <  0) p.x =  0;
    if      (p.y > DY) p.y = DY;
    else if (p.y <  0) p.y =  0;

    return p;
}

QTime MainWindow::getVideoTime(){
    QTime time(0,0,0);
    time = time.addSecs(video.get(CAP_PROP_POS_FRAMES)/video.get(CAP_PROP_FPS));
    return time;
}

QTime MainWindow::getVideoDuration(){
    QTime duration(0,0,0);
    duration = duration.addSecs(video.get(CAP_PROP_FRAME_COUNT)/video.get(CAP_PROP_FPS));
    return duration;
}

int getQuad(cv::Point p1, cv::Point p2){
    int quad = 0b1111;

    // 4 | 1
    // - - -
    // 3 | 2

    if(p2.x - p1.x >  20) quad &= 0b0011;
    if(p2.x - p1.x < -20) quad &= 0b1100;
    if(p2.y - p1.y >  20) quad &= 0b0110;
    if(p2.y - p1.y < -20) quad &= 0b1001;

    return quad;
}

bool inZone(Mat& animal, Mat& zone, float precision = 0.75){
    int nPixels = (float)countNonZero(animal)*precision;
    Mat animalIn;
    //bitwise_and(animal, zone, animalIn);
    animal.copyTo(animalIn, zone);
    if( countNonZero(animalIn) >= nPixels ) return true;
    else return false;
}

/*** ABRIR VIDEO ***/
void MainWindow::on_actionOpenVideo_triggered()
{
    QString videoFilePath = QFileDialog::getOpenFileName(
                this,
                tr("Abrir Arquivo"),
                "C://",
                "Videos (*.mp4 *.avi *.mov *.wmv)"
                );

    if(!video.open(videoFilePath.toStdString())) return;

    // Thumbnail
    video >> frame;
    cvtColor(frame, frame, COLOR_BGR2RGB);
    QImage qimgVideoThumb(frame.data,
                          frame.cols,
                          frame.rows,
                          frame.step,
                          QImage::Format_RGB888);
    QGraphicsPixmapItem videoPixThumb( QPixmap::fromImage(qimgVideoThumb) );
    ui->videoList->addItem(new QListWidgetItem(QIcon(videoPixThumb.pixmap()), videoFilePath));

    videoFiles.push_back(videoFilePath);
}
// Botao
void MainWindow::on_btnAddVideo_pressed() { ui->actionOpenVideo->trigger(); }

//*** CONFIGURACAO ***//
void MainWindow::config(){

    // Processo inicial --------------------------------------------------------------------------------------------
    maskZoneGlobal.create(frame.size(), CV_8UC1);
    maskZoneGlobal = Scalar::all(1);

    ui->graphicsViewPlayer->setSceneRect(0, 0, frame.cols, frame.rows);
    ui->framePosSlider->setMaximum(video.get(CAP_PROP_FRAME_COUNT) - 1);

    ui->startTime->setMaximumTime(getVideoDuration());
    ui->endTime->setMaximumTime(getVideoDuration());
    ui->endTime->setTime(getVideoDuration());

    ui->tabWidget->setTabEnabled(2, true);
    ui->tabWidget->setCurrentIndex(2);

    ui->toolBox->setCurrentIndex(0);
    setBackground(ui->nSamples->value());
    // -------------------------------------------------------------------------------------------------------------

    Mat frameCopy;
    Point p;

    while(video.isOpened() && !lock){

        if(ui->graphicsViewPlayer->underMouse())
            QApplication::setOverrideCursor(Qt::BlankCursor);
        else QApplication::setOverrideCursor(Qt::ArrowCursor);
        p = getFrameCursor(*ui->graphicsViewPlayer, frame);

        if(!pause){
            video >> frame;
            cvtColor(frame, frame, COLOR_BGR2RGB);
        }

        if(!frame.empty())
        {

            // Selecao se pausado -------------------------------------------------
            if(pause){
                frameCopy = frame.clone();

                if(QApplication::mouseButtons() == Qt::LeftButton
                && (tool != -1)
                && (tool != TOOL_POLYGON)
                && (p.x > 0)
                && (p.x < frame.cols)
                && (p.y > 0)
                && (p.y < frame.rows) ){
                    if(!flagSelectPoints){
                        selectPoints[0] = getFrameCursor(*ui->graphicsViewPlayer, frame);
                        selectPoints[1] = getFrameCursor(*ui->graphicsViewPlayer, frame);
                        flagSelectPoints = true;
                    }else{
                        selectPoints[1] = getFrameCursor(*ui->graphicsViewPlayer, frame);
                    }
                }else flagSelectPoints = false;

                if(norm(selectPoints[0]-selectPoints[1]) > 0){

                    //linha
                    if(tool == TOOL_LINE)
                        line(frame, selectPoints[0], selectPoints[1], CV_RGB(0,0,255), 2);

                    //flecha
                    if(tool == TOOL_ARROW)
                        arrowedLine(frame, selectPoints[0], selectPoints[1], CV_RGB(0,0,255), 2);

                    //retangulo
                    if(tool == TOOL_RECTANGLE)
                        rectangle(frame, selectPoints[0], selectPoints[1], CV_RGB(0,255,0), 2);

                    //circulo
                    if(tool == TOOL_CIRCLE)
                        circle(frame, (selectPoints[0]+selectPoints[1])*.5, norm(selectPoints[0]-selectPoints[1])/2, CV_RGB(255,0,0), 2);
                }

                //poligono
                if(tool == TOOL_POLYGON){
                    if(QApplication::mouseButtons() == Qt::LeftButton){
                        if(!flagSelectPolygon
                        && (p.x > 0)
                        && (p.x < frame.cols)
                        && (p.y > 0)
                        && (p.y < frame.rows) )
                        {
                            selectPolygon.push_back( getFrameCursor(*ui->graphicsViewPlayer, frame) );
                            flagSelectPolygon = true;
                        }
                    }   else flagSelectPolygon = false;

                    if(QApplication::mouseButtons() == Qt::RightButton){
                        if(!flagSelectPolygon2 && !selectPolygon.empty()){
                            selectPolygon.pop_back();
                            flagSelectPolygon2 = true;
                        }
                    }else   flagSelectPolygon2 = false;

                    polylines(frame, selectPolygon, true, CV_RGB(0,255,0), 2);
                }   else selectPolygon.clear();


                if(!ui->btnConfirm->isDown() && mode !=0){
                    if( ((norm(selectPoints[0]-selectPoints[1]) > 0) || (!selectPolygon.empty())) )
                    {
                        // MEDIDA -----------------------------------------------------------------
                        if(mode == MODE_MEASSURE && QApplication::mouseButtons() == Qt::LeftButton){
                            putText(frame,
                                    to_string((int)norm(selectPoints[0]-selectPoints[1])) + "px",
                                    getFrameCursor(*ui->graphicsViewPlayer, frame),
                                    FONT_HERSHEY_PLAIN,
                                    2, CV_RGB(255,255,0), 2);
                        }

                        // SELECAO ----------------------------------------------------------------
                        if(mode == MODE_ZONE_GLOBAL || mode == MODE_ZONE_ADD || mode == MODE_ANIMAL_SELECT){
                            Mat maskSelection(frame.size(), CV_8UC1);
                            maskSelection = Scalar::all(0);
                            Mat frameMasked;

                            if(tool == TOOL_RECTANGLE)
                                maskSelection(Rect(selectPoints[0], selectPoints[1])).setTo(Scalar(255));
                            if(tool == TOOL_CIRCLE)
                                circle(maskSelection,
                                      (selectPoints[0]+selectPoints[1])*.5,
                                       norm(selectPoints[0]-selectPoints[1])/2,
                                       Scalar(255),
                                       FILLED);
                            if(tool == TOOL_POLYGON)
                                fillConvexPoly( maskSelection, selectPolygon, Scalar(1) );

                            frame.copyTo(frameMasked, maskSelection);
                            addWeighted(frame, 0.5, frameMasked, 0.5, 0, frame);

                            if(mode == MODE_ZONE_GLOBAL) maskZoneGlobal = maskSelection.clone();
                        }
                    }
                }


            }// -----------------------------------------------------------------------------------


            // indicador de tempo
            ui->labelTime->setText(getVideoTime().toString());

            // slider para ajustar o frame atual
            if(QApplication::mouseButtons() != Qt::LeftButton)
                ui->framePosSlider->setValue(video.get(CAP_PROP_POS_FRAMES));

            // mostrar zonas
            if(ui->checkShowZones->isChecked() && !zones.empty()){
                Scalar zoneColor; // Cor da zona selecionada na lista

                for(uint i=0; i<zones.size(); i++){
                    if(ui->zoneList->item(i)->isSelected()) zoneColor = Scalar(255, 0, 0);
                    else zoneColor = Scalar(0, 255, 0);

                    if(zones[i].type == SHAPE_POLYGON){
                        polylines(frame, zones[i].points, true, zoneColor, 2);
                    }else{
                        if(zones[i].type == SHAPE_RECTANGLE)
                            rectangle(frame, zones[i].points[0], zones[i].points[1], zoneColor, 2);
                        if(zones[i].type == SHAPE_CIRCLE)
                            circle(frame, (zones[i].points[0]+zones[i].points[1])*.5,
                                   norm(zones[i].points[0]-zones[i].points[1])/2, zoneColor, 2);
                    }
                    putText(frame, to_string(i+1), Point(zones[i].points[0].x - 5, zones[i].points[0].y - 5),
                            FONT_HERSHEY_PLAIN, 2, zoneColor);
                }
            }

            // Mostra o frame -----------------
            if(pause) circle(frame, p, 3, CV_RGB(0,140,255), 2);

            QImage qimg(frame.data,
                        frame.cols,
                        frame.rows,
                        frame.step,
                        QImage::Format_RGB888);

            pixmapPlayer.setPixmap( QPixmap::fromImage(qimg) );
            ui->graphicsViewPlayer->fitInView(&pixmapPlayer, Qt::KeepAspectRatio);

            if(pause) frame = frameCopy.clone();
            // --------------------------------

            // CONFIGURACAO DA COBAIA -------------------------------------------------
            while(mode == MODE_ANIMAL_EDIT){
                Mat animal2, animalMask;
                vector<vector<Point>> animalContours;
                vector<Vec4i> animalHierarchy;
                frame(Rect(selectPoints[0], selectPoints[1])).copyTo(animal);
                animalMask.create(animal.size(), CV_8UC1);

                if(!background.empty()){
                    subtract(animal, background((Rect(selectPoints[0], selectPoints[1]))), animalMask);
                    cvtColor(animalMask, animalMask, COLOR_RGB2GRAY);
                    threshold(animalMask, animalMask, ui->adjustSlider->value(), 255, THRESH_BINARY);
                    GaussianBlur(animalMask, animalMask, Size(3,3), 0, 0);
                }

                animal.copyTo(animal2);
                animal = Scalar::all(0);
                animal2.copyTo(animal, animalMask);

                // desenha os contornos
                findContours(animalMask, animalContours, animalHierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE, Point(0,0));
                if(!animalContours.empty()){
                    sortContours(animalContours); // ordena os contornos por area

                    approxPolyDP( animalContours[0], animalContour, 0.005 * arcLength(animalContours[0], true), true );
                    animalContours[0] = animalContour;

                    drawContours(animal, animalContours, 0, CV_RGB(0,140,255), 1, 8, animalHierarchy, 0, Point());
                }

                QImage qimg(animal.data,
                            animal.cols,
                            animal.rows,
                            animal.step,
                            QImage::Format_RGB888);

                pixmapPlayer.setPixmap( QPixmap::fromImage(qimg) );
                ui->graphicsViewPlayer->fitInView(&pixmapPlayer, Qt::KeepAspectRatio);
                qApp->processEvents();
            }// -----------------------------------------------------------------------


            // video chega no final
            if(video.get(CAP_PROP_POS_FRAMES) == video.get(CAP_PROP_FRAME_COUNT)){
                pause = true;
                video.set(CAP_PROP_POS_FRAMES, 0);
            }

        }   qApp->processEvents();
    }
}
// Botao
void MainWindow::on_btnConfig_pressed() { config(); }
void MainWindow::on_videoList_itemClicked(){ ui->btnConfig->setEnabled(true); }
void MainWindow::on_videoList_itemDoubleClicked(){ config(); }

//*** ANALISE ***//
void MainWindow::on_startBtn_pressed()
{
    /*if(video.isOpened() && lock){
        ui->startBtn->setText("Reiniciar Análise");
        video.release();
        lock = false;
        return;
    }*/

    if(background.empty() || animalContour.empty()){
        ui->statusBar->showMessage("Defina o background e a cobaia primeiro!");
        return;
    }

    video.open(videoFiles[ui->videoList->currentRow()].toStdString());

    ui->tabWidget->setTabEnabled(3,true);
    ui->tabWidget->setCurrentIndex(3);

    if(ui->checkShowDistance->isChecked()) ui->distanceBox->show();
    ui->tabWidget->setTabEnabled(2, false);

    // frames de inicio e termino -------------------------
    uint startTimeSecs = ui->startTime->time().hour()   * 3600 +
                         ui->startTime->time().minute() * 60 +
                         ui->startTime->time().second();
    uint endTimeSecs =   ui->endTime->time().hour()   * 3600 +
                         ui->endTime->time().minute() * 60 +
                         ui->endTime->time().second();

    startFrame = startTimeSecs * video.get(CAP_PROP_FPS);
    endFrame = endTimeSecs     * video.get(CAP_PROP_FPS);

    video.set(CAP_PROP_POS_FRAMES, startFrame);

    // Zonas ----------------------------------------------
    int countIn = -1;

    // Mapas ----------------------------------------------
    if(ui->checkCreateTrackMap->isChecked())
        ui->viewSelector->addItem("trajeto");

    if(ui->checkCreateHeatMap->isChecked())
        ui->viewSelector->addItem("heatmap");
    // ----------------------------------------------------

    ui->progressBar->setMinimum(startFrame);
    ui->progressBar->setMaximum(endFrame);
    ui->slider->setValue(ui->adjustSlider->value());
    trackImg.create(frame.size(), frame.type());
    trackImg = Scalar::all(0);

    int heatmapCount[frame.rows][frame.cols];
    for(int i=0; i<frame.rows; i++)
        for(int j=0; j<frame.cols; j++)
            heatmapCount[i][j] = 0;
    int heatmapMaxNum = 255;
    Mat heatmapGray;
    heatmapGray.create(frame.size(), CV_8UC1);
    heatmapGray = Scalar::all(0);
    heatmap.create(frame.size(), frame.type());
    heatmap = Scalar::all(0);

    lock = true;

    // Variaveis ------------
    Mat frame2, frame_tmp, movement;
    const int margin = 15, learnSizeCount = 100, nPts = 50;
    int movementCount = 0, smallObjectArea = 0;
    int gtIndexContour = 0;
    float avArea, avAreaSum = 0;
    bool isCalibrated = false;
    vector<vector<Point>> contours, contours_poly, movementContours;
    vector<Point> pts[nPts];
    vector<Vec4i> hierarchy, movementHierarchy;
    vector<Rect> boundRect;
    vector<Moments> mu;                // moments
    vector<Point2f> mc;                // centro
    vector<Point> trackPath, lastContour;
    uchar value;
    vector<float> area;

    //velocidade
    const uint maxVelPts = video.get(CAP_PROP_FPS);
    vector<Point> velPts;
    float currentVel = 0.0, maxVel = 0;

    // mapeamento da orientacao da cobaia
    const int orientationDist = 30;  //        30 pixels
    Point firstOrientationPoint;    // ponto1 <---------> ponto2
    Mat orientationMap;             // mapa contendo as direcoes

    //           Cores:
    const Scalar red   = Scalar(255, 0, 0),
                 cyan  = Scalar(0, 255, 255);
    // ----------------------

    while(video.isOpened())
    {
        video >> frame;
        value = ui->slider->value();
        ui->progressBar->setValue(video.get(CAP_PROP_POS_FRAMES));

        if(!frame.empty())
        {
            if(!smallObjectArea) smallObjectArea = (frame.rows*frame.cols)*0.003;
            frame.copyTo(frame2, maskZoneGlobal);

            // frame2 - background
            subtract(frame2, background, frame2);

            // Conversao de cores
            cvtColor(frame, frame, COLOR_BGR2RGB);
            cvtColor(frame2, frame2, COLOR_BGR2GRAY);

            // Aplica o threshold
            threshold(frame2, frame2, value, 255, THRESH_BINARY);
            GaussianBlur(frame2, frame2, Size(3,3), 0, 0);

            // Deteccao de movimento ------------------------------------------
            if(movementCount == 1){
                frame2.copyTo(frame_tmp);
            }else if(movementCount == 10){
                subtract(frame2, frame_tmp, movement);
                movementCount = 0;
            }   movementCount++;

            findContours(movement, movementContours, movementHierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE, Point(0,0));

            vector<Rect> movementBoundRect = vector<Rect>(movementContours.size());
            Point2f movementCenter;
            if(movementContours.size() > 0){
                sortContours(movementContours);

                for(uint i=0; i<movementContours.size(); i++){
                    movementBoundRect[i] = boundingRect( Mat(movementContours[i]) );
                    if(ui->checkShowMovement->isChecked())
                        drawContours(frame, movementContours, i, CV_RGB(255,0,204), 2, 8, hierarchy, 0, Point());
                }

                Moments movementMoment = moments(movementContours[0], false);
                movementCenter = Point2f(movementMoment.m10 / movementMoment.m00,
                                         movementMoment.m01 / movementMoment.m00);
            }
            // ----------------------------------------------------------------


            // Encontra e desenha os contornos ------------------------------------------------------------------------
            findContours(frame2, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE, Point(0,0));

            // organiza os contornos por maior area
            sortContours(contours);
            // remove os contornos com area pequena
            for(uint i=0; i<contours.size(); i++){
                if(contourArea(contours[i]) < smallObjectArea){
                    contours.erase(contours.begin() + i, contours.end());
                    break;
                }
            }

            // usa a ultima posicao caso nao haja contornos
            if(contours.empty() && !lastContour.empty()){
                contours.push_back(lastContour);
            }

            contours_poly = vector<vector<Point>>(contours.size());
            boundRect = vector<Rect>(contours.size());
            mu = vector<Moments>(contours.size());
            mc = vector<Point2f>(contours.size());
            // Elipses e retangulos rotacionados para ocupar a menor area possivel
            vector<RotatedRect> minRect( contours.size() );
            vector<RotatedRect> minEllipse( contours.size() );

            for(uint i=0; i<contours.size(); i++){
                approxPolyDP( contours[i], contours_poly[i], 0.005 * arcLength(contours[i], true), true );
                boundRect[i] = boundingRect( Mat(contours_poly[i]) );
                mu[i] = moments(contours_poly[i], false);
                mc[i] = Point2f(mu[i].m10 / mu[i].m00, mu[i].m01 / mu[i].m00);
                minRect[i] = minAreaRect( Mat(contours[i]) );
                if( contours[i].size() > 5 ){
                    minEllipse[i] = fitEllipse( Mat(contours[i]) );
                }
            }


            if(!contours.empty())
            {
                gtIndexContour = 0;

                if(pts[0].size() == learnSizeCount-1)
                {
                    if(!isCalibrated){
                        isCalibrated = true;
                        for(uint i=1; i<pts[0].size()+1; i++){
                            float ptDistance = norm(pts[0][i] - pts[0][i+1]);
                            if (ptDistance > 5){
                                pts[0].erase(pts[0].begin()+i);
                                i--;
                            }
                        }
                    }

                    // remove o primeiro ponto caso o vetor atinja o limite
                    for(int i=0; i<nPts; i++)
                        pts[i].erase(pts[i].begin());

                    avAreaSum -= area[0];
                    area.erase(area.begin());
                }

                if(!isCalibrated) putText(frame, "Calibrando", Point(0, frame.rows), FONT_HERSHEY_PLAIN, 4, red, 3);


                // adiciona os valores ao vetor de pontos -----------------------------------------
                for(uint i=0; i<nPts; i++){
                    if(contours.size() > gtIndexContour + i)
                        pts[i].push_back(mc[gtIndexContour + i]);
                    else pts[i].push_back(mc[gtIndexContour]);

                    if(pts[i].size() > 1)
                        if(abs(norm(pts[i][pts[i].size()-1] - pts[i][pts[i].size()-2])) < 0.02){
                            pts[i].pop_back();
                        }
                } // ------------------------------------------------------------------------------


                // precisao -----------------------------------------------------------------
                float gtQ = 0, Q;

                if(pts[0].size() > 0)
                {
                    for(uint i=0; i<nPts; i++){
                        if((contours.size() > i))
                        {
                            float mdistance = abs(norm(pts[i][pts[i].size()-1] - Point(movementCenter)));
                            Q = 1/((matchShapes(animalContour, contours_poly[i], 1, 0.0) + mdistance)/2);

                            if( Q > gtQ ){
                                gtQ = Q;
                                gtIndexContour = i;
                                pts[0][pts[0].size()-1] = pts[i][pts[i].size()-1];
                            }

                            //ui->statusBar->showMessage(QString::number(contours.size()) + " " + QString::number(gtQ));
                        }//else break;
                    }
                } // ------------------------------------------------------------------------------

                // area media da cobaia (precisa estar depois do anti-teleporte)
                float currentArea = contourArea(contours_poly[gtIndexContour]);
                area.push_back(currentArea);
                avAreaSum += currentArea;
                avArea = avAreaSum / area.size();

                // ----------------------------------------------------------------------------------------------------


                //trajeto
                if(ui->checkCreateTrackMap->isChecked() && pts[0].size() > 1 && isCalibrated){
                    uint trackColor = (((video.get(CAP_PROP_POS_FRAMES) - startFrame)*255)/endFrame);
                    line(trackImg, pts[0][1], pts[0][2], Scalar(trackColor,0,255-trackColor), 2);
                }
                if(ui->checkShowTrack->isChecked()){
                    polylines(frame, pts[6], false, CV_RGB(255,255,255), 1);
                    polylines(frame, pts[5], false, CV_RGB(0,255,255), 1);
                    polylines(frame, pts[4], false, CV_RGB(255,0,255), 1);
                    polylines(frame, pts[3], false, CV_RGB(255,255,0), 1);
                    polylines(frame, pts[2], false, CV_RGB(0,0,255), 1);
                    polylines(frame, pts[1], false, CV_RGB(255,0,0), 1);
                    polylines(frame, pts[0], false, CV_RGB(0,255,0), 3);
                }

                //eventos
                for(uint i=0; i<events.size(); i++){
                    struct subevent sEv = events[i].subevents[events[i].subEventsCount];

                    if(sEv.type == EV_ZONE_ENTRY){
                        if(inZone(frame2, zones[sEv.intParam].zoneMat) && !zones[sEv.intParam].inZone){
                            if(events[i].subEventsCount == 0)
                                events[i].t_start.push_back(video.get(CAP_PROP_FPS)/video.get(CAP_PROP_FRAME_COUNT));

                            events[i].subEventsCount++;
                        }
                    }
                    if(sEv.type == EV_ZONE_EXIT){
                        if(!inZone(frame2, zones[sEv.intParam].zoneMat) && zones[sEv.intParam].inZone){
                            if(events[i].subEventsCount == 0)
                                events[i].t_start.push_back(video.get(CAP_PROP_FPS)/video.get(CAP_PROP_FRAME_COUNT));

                            events[i].subEventsCount++;
                        }
                    }

                    if(events[i].subEventsCount == events[i].subevents.size()){
                        events[i].subEventsCount = 0;

                        // indica que o evento ocorreu
                        events[i].t_stop.push_back(video.get(CAP_PROP_FPS)/video.get(CAP_PROP_FRAME_COUNT));
                    }
                }

                ui->statusBar->showMessage(QString::number(events[0].t_start.size()));

                //zonas
                if(countIn == -1){
                    countIn = 0;

                    for(uint i=0; i<zones.size(); i++){
                        if(inZone(frame2, zones[i].zoneMat)){
                              zones[i].inZone = true;
                              countIn++;
                        }else zones[i].inZone = false;
                    }
                }else{
                    for(uint i=0; i<zones.size(); i++){
                        //entrada
                        if(inZone(frame2, zones[i].zoneMat) && !zones[i].inZone){
                            zones[i].nEntry++;
                            zones[i].inZone = true;
                        }
                        //saida
                        if(!inZone(frame2, zones[i].zoneMat) && zones[i].inZone){
                            zones[i].nExit++;
                            zones[i].inZone = false;
                        }
                    }
                }
                // mostrar zonas
                if(ui->checkShowAllZones->isChecked() && !zones.empty()){
                    Scalar zoneColor; // Cor da zona selecionada na lista

                    for(uint i=0; i<zones.size(); i++){
                        if(zones[i].inZone) zoneColor = Scalar(0, 255, 0);
                        else zoneColor = Scalar(255, 0, 0);

                        if(zones[i].type == SHAPE_POLYGON){
                            polylines(frame, zones[i].points, true, zoneColor, 2);
                        }else{
                            if(zones[i].type == SHAPE_RECTANGLE)
                                rectangle(frame, zones[i].points[0], zones[i].points[1], zoneColor, 2);
                            if(zones[i].type == SHAPE_CIRCLE)
                                circle(frame, (zones[i].points[0]+zones[i].points[1])*.5,
                                       norm(zones[i].points[0]-zones[i].points[1])/2, zoneColor, 2);
                        }
                        putText(frame, to_string(i+1), Point(zones[i].points[0].x - 5, zones[i].points[0].y - 5),
                                FONT_HERSHEY_PLAIN, 2, zoneColor);
                    }
                }

                //heatmap
                if(ui->checkCreateHeatMap->isChecked()){
                    for(int i=0; i<frame2.rows; i++){
                        for(int j=0; j<frame2.cols; j++){
                            heatmapCount[i][j] += (bool)frame2.at<uchar>(i,j);
                            if(heatmapCount[i][j] > heatmapMaxNum)
                                heatmapMaxNum = heatmapCount[i][j];
                        }
                    }
                    for(int i=0; i<frame2.rows; i++){
                        for(int j=0; j<frame2.cols; j++){
                            heatmapGray.at<uchar>(i,j) = floor(heatmapCount[i][j]*(255/(float)heatmapMaxNum));
                        }
                    }
                }
                bitwise_not ( heatmapGray, heatmap );
                applyColorMap(heatmap, heatmap, COLORMAP_JET);

                //contorno:
                if(ui->checkShowContour->isChecked())
                    drawContours(frame, contours_poly, gtIndexContour, CV_RGB(0,140,255), 2, 8, hierarchy, 0, Point());

                //boundingbox:
                if(ui->checkShowRect->isChecked())
                {
                    // Bounding box
                    rectangle(frame, Point(boundRect[gtIndexContour].tl().x - margin, boundRect[gtIndexContour].tl().y - margin),
                                     Point(boundRect[gtIndexContour].br().x + margin, boundRect[gtIndexContour].br().y + margin),
                                     cyan, 2, 8, 0);

                    // Elipse + Retangulo rotacionados
                    ellipse( frame, minEllipse[gtIndexContour], cyan, 2, 8 );
                    Point2f rect_points[4]; minRect[gtIndexContour].points( rect_points );

                    for( int j = 0; j < 4; j++ )
                        line( frame, rect_points[j], rect_points[(j+1)%4], cyan, 1, 8 );

                    //orientacao
                    if(orientationMap.empty()) orientationMap.create(frame.size(), frame.type());

                    uint quad = 0;

                    if(norm(firstOrientationPoint - pts[0][pts[0].size()-1]) > orientationDist){
                        quad = getQuad(firstOrientationPoint, pts[0][pts[0].size()-1]);
                        arrowedLine(orientationMap, firstOrientationPoint, pts[0][pts[0].size()-1], CV_RGB(255,255,0), 2);
                        firstOrientationPoint = pts[0][pts[0].size()-1];
                    }
                    if(quad & 1){
                        line(frame, Point(frame.cols/2, 0), Point(frame.cols, 0), CV_RGB(0,0,255), 8);
                        line(frame, Point(frame.cols, 0), Point(frame.cols, frame.rows/2), CV_RGB(0,0,255), 8);
                    }
                    if(quad & 2){
                        line(frame, Point(frame.cols, frame.rows/2), Point(frame.cols, frame.rows), CV_RGB(0,0,255), 8);
                        line(frame, Point(frame.cols, frame.rows), Point(frame.cols/2, frame.rows), CV_RGB(0,0,255), 8);
                    }
                    if(quad & 4){
                        line(frame, Point(frame.cols/2, frame.rows), Point(0, frame.rows), CV_RGB(0,0,255), 8);
                        line(frame, Point(0, frame.rows), Point(0, frame.rows/2), CV_RGB(0,0,255), 8);
                    }
                    if(quad & 8){
                        line(frame, Point(0, frame.rows/2), Point(0, 0), CV_RGB(0,0,255), 8);
                        line(frame, Point(0, 0), Point(frame.cols/2, 0), CV_RGB(0,0,255), 8);
                    }
                }

                //centro
                if(ui->checkShowPoint->isChecked())
                    circle(frame, pts[0][pts[0].size()-1], 5, red, -1, 8, 0);


                //distancia percorrida
                trackPath.push_back(pts[0][pts[0].size()-1]);
                if(pixelsPerMeter != 0){
                    ui->distanceLabel->setText(QString::number(arcLength(trackPath,false)/pixelsPerMeter)+"m");

                    //velocidade
                    velPts.push_back(pts[0][pts[0].size()-1]);
                    if(velPts.size() > maxVelPts){
                        velPts.erase(velPts.begin());
                        currentVel = norm(velPts[0] - velPts[velPts.size()-1]);
                        if(currentVel > maxVel) maxVel = currentVel;
                    }
                    ui->maxVelLabel->setText(QString::number(maxVel/pixelsPerMeter) + " m/s");
                    ui->currentVelLabel->setText(QString::number(currentVel/pixelsPerMeter) + " m/s");
                }


                // copia o contorno da cobaia
                lastContour = contours[gtIndexContour];
            }

            // Mostra o frame --------------------------------------------------------------------
            Mat *frameAlternative = nullptr;

            if(ui->viewSelector->currentText() == "tracking") frameAlternative = &frame;
            if(ui->viewSelector->currentText() == "máscara") frameAlternative = &frame2;
            if(ui->viewSelector->currentText() == "movimento") frameAlternative = &movement;
            if(ui->viewSelector->currentText() == "trajeto") frameAlternative = &trackImg;
            if(ui->viewSelector->currentText() == "orientação") frameAlternative = &orientationMap;
            if(ui->viewSelector->currentText() == "heatmap") frameAlternative = &heatmap;

            QImage::Format formatAlternative = (frameAlternative->channels() == 1) ? QImage::Format_Grayscale8 :
                                               (frameAlternative->channels() == 3) ? QImage::Format_RGB888 :
                                                                                    QImage::Format_RGBA8888;
            QImage qimg(frameAlternative->data,
                                   frameAlternative->cols,
                                   frameAlternative->rows,
                                   frameAlternative->step,
                                   formatAlternative);
            pixmap.setPixmap( QPixmap::fromImage(qimg) );
            ui->graphicsView->fitInView(&pixmap, Qt::KeepAspectRatio);
            // -----------------------------------------------------------------------------------------------

            // Fim da analise
            if(video.get(CAP_PROP_POS_FRAMES) == endFrame){
                video.release();
                ui->startBtn->setText("Reiniciar Análise");
                ui->actionFinish->trigger();
            }

            // reseta a trajetoria
            //if(ui->btnResetTrack->isDown()){
            //    for(int i=0; i<nPts; i++)
            //        pts[i].clear();
            //}
        }
        qApp->processEvents();
        /*while(ui->pauseBtn->isChecked()){
            ui->pauseBtn->setText("Continuar Análise");
            qApp->processEvents();
            if(ui->nextBtn->isDown()) break;
        }   ui->pauseBtn->setText("Pausar Análise");*/
    }
}

//*** FIM DA ANALISE ***//
void MainWindow::on_actionFinish_triggered()
{
    //*** IMAGENS PARA RESULTADOS ***//

    //heatmap
    QImage qimgHeatmap(heatmap.data,
                       heatmap.cols,
                       heatmap.rows,
                       heatmap.step,
                       QImage::Format_RGB888);

    QGraphicsPixmapItem resultPixHeatmap;
    resultPixHeatmap.setPixmap( QPixmap::fromImage(qimgHeatmap) );
    ui->resultLabelHeatmap->setPixmap(resultPixHeatmap.pixmap());

    //trackmap
    QImage qimgTrackmap(trackImg.data,
                        trackImg.cols,
                        trackImg.rows,
                        trackImg.step,
                        QImage::Format_RGB888);

    QGraphicsPixmapItem resultPixTrackmap;
    resultPixTrackmap.setPixmap( QPixmap::fromImage(qimgTrackmap) );
    ui->resultLabelTrackmap->setPixmap(resultPixTrackmap.pixmap());

    // Abre a aba de resultados
    ui->tabWidget->setTabEnabled(4, true);
    ui->tabWidget->setCurrentIndex(4);
}

// Impede o programa de fechar caso o video ainda esteja em execucao
void MainWindow::closeEvent(QCloseEvent *event)
{
    if(video.isOpened() && lock){
        QMessageBox::warning(this, "Atenção", "Pare a análise antes de fechar o programa!");
        event->ignore();
    }else{
        video.release();
        event->accept();
    }
}

// Play/Pause
void MainWindow::on_btnPlayPause_toggled(bool checked){ pause = !checked; }

// Trackbar do player de video
void MainWindow::on_framePosSlider_sliderReleased()
{
    if(video.isOpened()){
        video.set(CAP_PROP_POS_FRAMES, ui->framePosSlider->value());
        video >> frame;
        cvtColor(frame, frame, COLOR_BGR2RGB);
    }
}

// Extracao do background (amostras)
void MainWindow::on_btnSetBackground_pressed() { setBackground(ui->nSamples->value()); }
// Extracao do background (frame atual)
void MainWindow::on_btnSetFrameBackground_pressed() { setBackground(frame); }

// Definicao do instante de inicio da analise
void MainWindow::on_btnStartTimeSetInstant_pressed() { ui->startTime->setTime(getVideoTime()); }
// Definicao do instante de termino da analise
void MainWindow::on_btnEndTimeSetInstant_pressed() { ui->endTime->setTime(getVideoTime()); }

void MainWindow::on_btnAnimalSelect_pressed()
{
    ui->actionToolRectangle->trigger();
    ui->confirmBox->show();
    ui->actionOpenVideo->setEnabled(false);
    ui->actionToolCircle->setEnabled(false);
    ui->actionToolLine->setEnabled(false);
    ui->actionToolArrow->setEnabled(false);
    ui->actionToolPolygon->setEnabled(false);

    mode = MODE_ANIMAL_SELECT;
}

void MainWindow::on_btnMeassure_pressed()
{
    ui->actionToolArrow->trigger();
    ui->confirmBox->show();
    ui->actionOpenVideo->setEnabled(false);
    ui->actionToolCircle->setEnabled(false);
    ui->actionToolLine->setEnabled(false);
    ui->actionToolPolygon->setEnabled(false);
    ui->actionToolRectangle->setEnabled(false);

    mode = MODE_MEASSURE;
}

//*** Caixa de confirmacao ***//
void MainWindow::on_btnConfirm_pressed()
{
    if( (norm(selectPoints[0] - selectPoints[1]) == 0) && (selectPolygon.size() < 3) ){
        ui->statusBar->showMessage("Selecione uma área!", 5000);
        return;
    }

    if(mode == MODE_MEASSURE){
        pixelsPerMeter =  norm(selectPoints[0] - selectPoints[1]) /
                          QInputDialog::getDouble(this, "Medida", "Valor em metros:", 0.0, 0, 999, 3);
        ui->labelPixelsPerMeter->setText(QString::number(pixelsPerMeter, ' ', 2) + " pixels/metro");

        ui->meassureResultBox->setEnabled(true);
    }

    if(mode == MODE_ZONE_GLOBAL){
        if(tool == TOOL_POLYGON){
            zoneGlobal.type = SHAPE_POLYGON;
            zoneGlobal.points = selectPolygon;
        }else{
            if(tool == TOOL_RECTANGLE) zoneGlobal.type = SHAPE_RECTANGLE;
            if(tool == TOOL_CIRCLE)    zoneGlobal.type = SHAPE_CIRCLE;

            zoneGlobal.points.push_back(selectPoints[0]);
            zoneGlobal.points.push_back(selectPoints[1]);
        }
    }

    if(mode == MODE_ZONE_ADD){
        struct zone temp_zone;

        temp_zone.zoneMat.create(frame.size(), CV_8UC1);
        temp_zone.zoneMat = Scalar::all(0);
        temp_zone.nEntry = temp_zone.nExit = 0;

        if(tool == TOOL_POLYGON){
            temp_zone.type = SHAPE_POLYGON;
            temp_zone.points = selectPolygon;
            polylines(temp_zone.zoneMat, selectPolygon, true, Scalar::all(255));

            Moments mu = moments(selectPolygon, false);
            Point2f polyCenter = Point2f(mu.m10 / mu.m00, mu.m01 / mu.m00);

            // preenche o contorno
            floodFill(temp_zone.zoneMat, polyCenter, Scalar::all(255));
        }else{
            if(tool == TOOL_RECTANGLE){
                temp_zone.type = SHAPE_RECTANGLE;
                rectangle(temp_zone.zoneMat, selectPoints[0], selectPoints[1], Scalar::all(255));
            }
            if(tool == TOOL_CIRCLE){
                temp_zone.type = SHAPE_CIRCLE;
                circle(temp_zone.zoneMat, (selectPoints[0]+selectPoints[1])*.5, norm(selectPoints[0]-selectPoints[1])/2, Scalar::all(255));
            }

            temp_zone.points.push_back(selectPoints[0]);
            temp_zone.points.push_back(selectPoints[1]);

            // preenche o contorno
            floodFill(temp_zone.zoneMat, Point(selectPoints[0].x + (selectPoints[1].x-selectPoints[0].x)/2,
                                     selectPoints[0].y + (selectPoints[1].y-selectPoints[0].y)/2), Scalar::all(255));
        }

        // Thumbnail
        QImage qimgZoneThumb(temp_zone.zoneMat.data,
                             temp_zone.zoneMat.cols,
                             temp_zone.zoneMat.rows,
                             temp_zone.zoneMat.step,
                             QImage::Format_Grayscale8);
        QGraphicsPixmapItem zonePixThumb( QPixmap::fromImage(qimgZoneThumb) );

        zones.push_back(temp_zone);
        ui->zoneList->addItem(new QListWidgetItem(QIcon(zonePixThumb.pixmap()),
                                                   "Zona " + QString::number(zones.size())));

        // Lista de zonas (resultado)
        QListWidgetItem* item = new QListWidgetItem("Zona " + QString::number(zones.size()));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable); // set checkable flag
        item->setCheckState(Qt::Unchecked); // AND initialize check state
        ui->zonesResultList->addItem(item);
    }

    if(mode == MODE_ANIMAL_SELECT){
        mode = MODE_ANIMAL_EDIT;
        tool = -1;

        ui->graphicsView->setSceneRect(0, 0, frame(Rect(selectPoints[0], selectPoints[1])).cols,
                                             frame(Rect(selectPoints[0], selectPoints[1])).rows);
        //ui->graphicsViewAlternative->setSceneRect(0, 0, frame(Rect(selectPoints[0], selectPoints[1])).cols,
        //                                                frame(Rect(selectPoints[0], selectPoints[1])).rows);

        ui->actionToolRectangle->setEnabled(false);
        ui->adjustSlider->show();
        return;
    }

    if(mode == MODE_ANIMAL_EDIT){
        ui->graphicsView->setSceneRect(0, 0, frame.cols, frame.rows);
        //ui->graphicsViewAlternative->setSceneRect(0, 0, frame.cols, frame.rows);
    }

    ui->actionOpenVideo->setEnabled(true);
    ui->actionToolCircle->setEnabled(true);
    ui->actionToolLine->setEnabled(true);
    ui->actionToolArrow->setEnabled(true);
    ui->actionToolPolygon->setEnabled(true);
    ui->actionToolRectangle->setEnabled(true);

    selectPoints[0] = selectPoints[1] = Point(0,0);
    selectPolygon.clear();

    ui->confirmBox->hide();
    ui->adjustSlider->hide();
    mode = MODE_DEFAULT;
}
void MainWindow::on_btnCancel_pressed()
{
    ui->actionOpenVideo->setEnabled(true);
    ui->actionToolCircle->setEnabled(true);
    ui->actionToolLine->setEnabled(true);
    ui->actionToolArrow->setEnabled(true);
    ui->actionToolPolygon->setEnabled(true);
    ui->actionToolRectangle->setEnabled(true);

    selectPoints[0] = selectPoints[1] = Point(0,0);
    ui->confirmBox->hide();
    mode = 0;
}

//*** FERRAMENTAS DE DESENHO ***//
void MainWindow::on_actionToolRectangle_triggered()
{
    tool = TOOL_RECTANGLE;

    selectPoints[0] = selectPoints[1] = Point(0,0);
    selectPolygon.clear();

    ui->actionToolRectangle->setChecked(true);
    ui->actionToolArrow->setChecked(false);
    ui->actionToolLine->setChecked(false);
    ui->actionToolCircle->setChecked(false);
    ui->actionToolPolygon->setChecked(false);
}
void MainWindow::on_actionToolCircle_triggered()
{
    tool = TOOL_CIRCLE;

    selectPoints[0] = selectPoints[1] = Point(0,0);
    selectPolygon.clear();

    ui->actionToolCircle->setChecked(true);
    ui->actionToolArrow->setChecked(false);
    ui->actionToolRectangle->setChecked(false);
    ui->actionToolLine->setChecked(false);
    ui->actionToolPolygon->setChecked(false);
}
void MainWindow::on_actionToolPolygon_triggered()
{
    tool = TOOL_POLYGON;

    selectPoints[0] = selectPoints[1] = Point(0,0);
    selectPolygon.clear();

    ui->actionToolPolygon->setChecked(true);
    ui->actionToolArrow->setChecked(false);
    ui->actionToolRectangle->setChecked(false);
    ui->actionToolLine->setChecked(false);
    ui->actionToolCircle->setChecked(false);
}
void MainWindow::on_actionToolArrow_triggered()
{
    tool = TOOL_ARROW;

    selectPoints[0] = selectPoints[1] = Point(0,0);
    selectPolygon.clear();

    ui->actionToolArrow->setChecked(true);
    ui->actionToolLine->setChecked(false);
    ui->actionToolRectangle->setChecked(false);
    ui->actionToolCircle->setChecked(false);
    ui->actionToolPolygon->setChecked(false);
}

//*** ZONAS ***//
void MainWindow::on_btnZoneAdd_pressed()
{
    ui->actionToolRectangle->trigger();
    ui->confirmBox->show();
    ui->actionOpenVideo->setEnabled(false);
    ui->actionToolLine->setEnabled(false);
    ui->actionToolArrow->setEnabled(false);

    mode = MODE_ZONE_ADD;
}
void MainWindow::on_btnZoneRemove_pressed()
{
    if(!zones.empty() && ui->zoneList->isItemSelected(ui->zoneList->currentItem())){
        zones.erase(zones.begin() + ui->zoneList->currentRow());
        ui->zoneList->removeItemWidget(ui->zoneList->currentItem());
        ui->zonesResultList->removeItemWidget(ui->zoneList->currentItem());
        delete ui->zoneList->currentItem();
        delete ui->zonesResultList->item(ui->zoneList->currentRow());
        for(uint i=0; i<zones.size(); i++){
            ui->zoneList->item(i)->setText("Zona " + QString::number(i + 1));
            ui->zonesResultList->item(i)->setText("Zona " + QString::number(i + 1));
        }
    }
}
void MainWindow::on_btnZoneMarkAll_pressed()
{
    for(uint i=0; i<zones.size(); i++)
        ui->zonesResultList->item(i)->setCheckState(Qt::Checked);
}
void MainWindow::on_btnZoneUnmarkAll_pressed()
{
    for(uint i=0; i<zones.size(); i++)
        ui->zonesResultList->item(i)->setCheckState(Qt::Unchecked);
}


//*** EVENTOS ***//
void MainWindow::on_subEventSelect_currentIndexChanged(const QString &arg1)
{
    //limpa os argumentos
    ui->subEventParam->hide();
    ui->subEventParam->clear();
    ui->subEventParamSelect->hide();
    ui->subEventParamSelect->clear();

    if(arg1 == "Entrada em Zona" || arg1 == "Saída de Zona"){
        ui->subEventParamSelect->show();

        ui->subEventParamSelect->addItem("Selecione");
        for(uint i=0 ; i<zones.size(); i++){
            ui->subEventParamSelect->addItem("Zona " + QString::number(i+1));
        }
    }
}

void MainWindow::on_btnAddSubEvent_pressed()
{
    if(ui->subEventSelect->currentIndex() == 0) return;

    struct subevent tmp_subevent;
    tmp_subevent.type = ui->subEventSelect->currentIndex();

    // entrada/saida de zonas
    if(tmp_subevent.type == EV_ZONE_ENTRY || tmp_subevent.type == EV_ZONE_EXIT){
        if(ui->subEventParamSelect->currentIndex() == 0) return;

        tmp_subevent.intParam = ui->subEventParamSelect->currentIndex()-1;
    }

    // adiciona o subevento na lista
    if(tmp_subevent.type == EV_ZONE_ENTRY)
        ui->subEventList->addItem("Entrada na Zona " + QString::number(tmp_subevent.intParam + 1));
    if(tmp_subevent.type == EV_ZONE_EXIT)
        ui->subEventList->addItem("Saída da Zona " + QString::number(tmp_subevent.intParam + 1));

    // se nao houver eventos eh criado um
    if(events.empty() || (!events.empty() && !events[events.size()-1].name.isEmpty())){
        struct event tmp_event;
        tmp_event.subEventsCount = 0;
        events.push_back(tmp_event);
    }

    events[events.size()-1].subevents.push_back(tmp_subevent);
}

void MainWindow::on_btnNewEvent_pressed()
{
    if(!events.empty() && !events[events.size()-1].subevents.empty()){
        ui->subEventList->clear();

        if(ui->eventNameInput->text().isEmpty())
            events[events.size()-1].name = "Evento " + QString::number(events.size());
        else events[events.size()-1].name = ui->eventNameInput->text();

        events[events.size()-1].allowBetween = !ui->checkAllowEventBetween->isChecked();
        ui->eventList->addItem(events[events.size()-1].name);
    }
}
