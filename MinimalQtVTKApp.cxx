#include <QVTKOpenGLNativeWidget.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkDICOMImageReader.h>
#include <vtkNamedColors.h>
#include <vtkImageSliceMapper.h>
#include <vtkImageThreshold.h>
#include <vtkImageGaussianSmooth.h>
#include <vtkMarchingCubes.h>
#include <vtkWindowedSincPolyDataFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkPolyDataNormals.h>
#include <vtkStripper.h>
#include <vtkPolyDataMapper.h>
#include <vtkCamera.h>
// needed to easily convert int to std::string
#include <sstream>
#include <vtkNew.h>

#include <QApplication>
#include <QDockWidget>
#include <QGridLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
#include <qfiledialog.h>

#include <cmath>
#include <cstdlib>

namespace {
	vtkActor* CreateTissue(vtkDICOMImageReader* reader, double ThrIn, double ThrOut, double colors[3]);
	void OpenFolder(QWidget* mainWidget, vtkDICOMImageReader* reader, vtkRenderer* renderer, 
		vtkProp* actor_lung, vtkProp* actor_blood, vtkProp* actor_skeleton, vtkGenericOpenGLRenderWindow* window);
} 

int main(int argc, char* argv[])
{
  QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
  QApplication app(argc, argv);
  // main window
  QMainWindow mainWindow;
  mainWindow.resize(1200, 900);
  // control area
  QDockWidget controlDock;
  mainWindow.addDockWidget(Qt::LeftDockWidgetArea, &controlDock);
  QLabel controlDockTitle("Control Dock");
  controlDockTitle.setMargin(20);
  controlDock.setTitleBarWidget(&controlDockTitle);

  QPointer<QVBoxLayout> dockLayout = new QVBoxLayout();
  QWidget layoutContainer;
  layoutContainer.setLayout(dockLayout);
  controlDock.setWidget(&layoutContainer);

  QPushButton openFolder;
  openFolder.setText("Open Folder");
  dockLayout->addWidget(&openFolder);
 
  // render area
  QPointer<QVTKOpenGLNativeWidget> vtkRenderWidget =
      new QVTKOpenGLNativeWidget();
  mainWindow.setCentralWidget(vtkRenderWidget);

  // VTK part
  //Update the base address of initial view
  std::string folder = "C:\\Users\\Laila\\Downloads\\New folder\\test\\ID00419637202311204720264";
  vtkNew<vtkDICOMImageReader> reader;
  reader->SetDirectoryName(folder.c_str());
  reader->Update();

  //Rendering data from reader
  vtkNew<vtkGenericOpenGLRenderWindow> window;
  vtkRenderWidget->setRenderWindow(window.Get());
  vtkNew<vtkRenderer> renderer;
  vtkNew<vtkNamedColors> colors;
  vtkProp *actor_lung = CreateTissue(reader, -900, -400, colors->GetColor3d("powder_blue").GetData());
  renderer->AddActor(actor_lung); 
  vtkProp *actor_blood = CreateTissue(reader, 0, 120, colors->GetColor3d("salmon").GetData());
  renderer->AddActor(actor_blood);
  vtkProp *actor_skeleton = CreateTissue(reader, 100, 2000, colors->GetColor3d("wheat").GetData());
  renderer->AddActor(actor_skeleton);
  renderer->SetBackground(1.0, 1.0, 1.0);
  renderer->ResetCamera();
  renderer->ResetCameraClippingRange();
  vtkCamera* camera = renderer->GetActiveCamera();
  //camera->Elevation(120);
  //camera->Roll(90);
  renderer->SetActiveCamera(camera);
  window->AddRenderer(renderer);

  QWidget* mainWidget = mainWindow.parentWidget();
  QVTKInteractor* window_interactor = vtkRenderWidget->interactor();
  
  // connect the buttons
  QObject::connect(&openFolder, &QPushButton::released,
                   [&]() { ::OpenFolder(mainWidget, reader, renderer, actor_lung, actor_blood, actor_skeleton, window); });
  
  mainWindow.show();

  return app.exec();
}

namespace {
void OpenFolder(QWidget* mainWidget, vtkDICOMImageReader* reader, vtkRenderer* renderer, 
	vtkProp* actor_lung, vtkProp* actor_blood, vtkProp* actor_skeleton, vtkGenericOpenGLRenderWindow* window)
{
	//Pass the directory address chosen and read the DICOM series
	QString folderName = QFileDialog::getExistingDirectory(mainWidget, "Open Image", "C:\\");
	std::string folder = folderName.toStdString();
	reader->SetDirectoryName(folder.c_str());
	reader->Update();

	vtkNew<vtkNamedColors> colors;
	renderer->RemoveActor(actor_lung);
	actor_lung = CreateTissue(reader, -900, -400, colors->GetColor3d("powder_blue").GetData());
	renderer->AddActor(actor_lung); 
	renderer->RemoveActor(actor_blood);
	actor_blood = CreateTissue(reader, 0, 120, colors->GetColor3d("salmon").GetData());
	renderer->AddActor(actor_blood);
	renderer->RemoveActor(actor_skeleton);
	actor_skeleton = CreateTissue(reader, 100, 2000, colors->GetColor3d("wheat").GetData());
	renderer->AddActor(actor_skeleton);
	renderer->ResetCamera();
	renderer->ResetCameraClippingRange();
	renderer->SetActiveCamera(renderer->GetActiveCamera());
	window->Render();
}
vtkActor* CreateTissue(vtkDICOMImageReader* reader, double ThrIn, double ThrOut, double color_data[3])
{
	//Seperate the data of the DICOM series to three tissues and view as 3D
	double isoValue = 127.5;
	vtkNew<vtkImageThreshold> selectTissue;
	selectTissue->ThresholdBetween(ThrIn, ThrOut);
	selectTissue->ReplaceInOn();
	selectTissue->SetInValue(255);
	selectTissue->ReplaceOutOn();
	selectTissue->SetOutValue(0);
	selectTissue->SetInputConnection(reader->GetOutputPort());
	selectTissue->Update();
	

	double gaussianStandardDeviation = 2.0;
	double gaussianRadius = 5;
	vtkNew<vtkImageGaussianSmooth> gaussian;
	gaussian->SetStandardDeviation(gaussianStandardDeviation, gaussianStandardDeviation, gaussianStandardDeviation);
	gaussian->GetRadiusFactors(gaussianRadius, gaussianRadius, gaussianRadius);
	gaussian->SetInputConnection(selectTissue->GetOutputPort());

	vtkNew<vtkMarchingCubes> mcubes;
	mcubes->SetInputConnection(gaussian->GetOutputPort());
	mcubes->ComputeScalarsOff();
	mcubes->ComputeGradientsOff();
	mcubes->ComputeNormalsOff();
	mcubes->SetValue(0, isoValue);

	vtkNew<vtkWindowedSincPolyDataFilter> smoother;
	smoother->SetInputConnection(mcubes->GetOutputPort());
	smoother->SetNumberOfIterations(5);
	smoother->BoundarySmoothingOff();
	smoother->FeatureEdgeSmoothingOff();
	smoother->SetFeatureAngle(60.0);
	smoother->SetPassBand(0.001);
	smoother->NonManifoldSmoothingOn();
	smoother->NormalizeCoordinatesOn();
	smoother->Update();

	vtkNew<vtkPolyDataNormals> normals;
	normals->SetInputConnection(smoother->GetOutputPort());
	normals->SetFeatureAngle(60.0);

	vtkNew<vtkStripper> stripper;
	stripper->SetInputConnection(normals->GetOutputPort());

	vtkNew<vtkPolyDataMapper> mapper_p;
	mapper_p->SetInputConnection(stripper->GetOutputPort());

	vtkActor* actor_fn = vtkActor::New();
	actor_fn->SetMapper(mapper_p);
	actor_fn->GetProperty()->SetColor(color_data);
	actor_fn->GetProperty()->SetSpecular(.5);
	actor_fn->GetProperty()->SetSpecularPower(10);
	return actor_fn;
}
} 
