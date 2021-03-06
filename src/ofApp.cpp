#include "ofApp.h"
using namespace pd;

#pragma mark setups

void ofApp::setupGLEnvironment(){
    ofSetBackgroundColor(0);
    ofEnableDepthTest();
    ofSetVerticalSync(true);
    ofSetCircleResolution(50);
    ofSetLineWidth(0.1);
    ofEnableAlphaBlending();
    ofSetFullscreen(true);
}

void ofApp::setupGLCamera(){
    camera.setFarClip(500);
    camera.setNearClip(0.01);
    
    lookAtAnimation.setPosition(ofPoint(-0.2, 0, 2.35));
    lookAtAnimation.setRepeatType(LOOP_BACK_AND_FORTH);
    lookAtAnimation.setCurve(EASE_IN_EASE_OUT);
    lookAtAnimation.animateTo(ofPoint(-0.6, 0, 3.2));

    cameraAnimation.setPosition(ofPoint(-3., 0.1, -0.7));
    cameraAnimation.setRepeatType(LOOP_BACK_AND_FORTH);
    cameraAnimation.setCurve(EASE_IN_EASE_OUT);
    cameraAnimation.animateTo(ofPoint(2.14, -0.1, -1.63));
}

void ofApp::setupGLBuffer(){
    pointCloud.setup();
    pastSpectrogram.setup();
    futureSpectrogram.setup(true, ofColor(200,200,200,255));
    guiEnabled = false;
    boxEnabled = false;
    gainContour.reserve(kKinectWidth);
    for(int i = 0; i < kKinectWidth ;i++){
        gainContour.emplace_back(static_cast<float>(i) / kHalfKinectWidthFloat-1.0, -1, 0.0);
    }
    gainContourVbo.setVertexData(&gainContour[0], kKinectWidth, GL_DYNAMIC_DRAW);
}

void ofApp::audioSetup(){
    ofSoundStreamSetup(kNumOutput, kNumInput, this, kSampleRate, ofxPd::blockSize()*8, 3);
    pdGainBuffer = std::vector<float>(kNumBins, 0.0);
    pdFeedbackSpectrumBuffer = std::vector<float>(kNumBins, 0.0);
    pdPastSpectrumBuffer = std::vector<float>(kNumBins, 0.0);
    pd.init(kNumOutput, kNumInput, kSampleRate);
    pd.openPatch(patchname);
    pd.start();
}

void ofApp::guiSetup(){
    gui.setup();
    gui.add(lookAtSlider.setup("lookat", ofVec3f(-0.2, 0, 2.35), ofVec3f(-10,-10,-10), ofVec3f(10,10,10)));
    gui.add(cameraPosSlider.setup("cameraPos", ofVec3f(-3., 0.1, -0.7), ofVec3f(-10,-10,-10), ofVec3f(10,10,10)));
    gui.add(distThresholdSlider.setup("dist thresh", 100, 0, 500));
    
    ofFile file(ofToDataPath("Settings.xml"));
    if(file.exists()){
        gui.loadFromFile(ofToDataPath("Settings.xml"));
    }
}

void ofApp::kinectSetup(){
    kinect.init(true,false,false); // we need only infrared
    kinect.open();if(kinect.isConnected()) {
        ofLogNotice() << "sensor-emitter dist: " << kinect.getSensorEmitterDistance() << "cm";
        ofLogNotice() << "sensor-camera dist:  " << kinect.getSensorCameraDistance() << "cm";
        ofLogNotice() << "zero plane pixel size: " << kinect.getZeroPlanePixelSize() << "mm";
        ofLogNotice() << "zero plane dist: " << kinect.getZeroPlaneDistance() << "mm";
    }else{
        ofLog() << "kinect not found";
        ofExit(1);
    }
}

void ofApp::setup(){
    ofEnableSmoothing();
    ofEnableAntiAliasing();
    kinectSetup();
    ofSetFrameRate(kTargetFPS);
    setupGLEnvironment();
    setupGLCamera();
    setupGLBuffer();
    audioSetup();
    guiSetup();
    pointCloud.setup();
    trigger = Trigger::Stay;
}

#pragma mark update;


float ofApp::updateGainContour(){

    static Trigger previousStat;
    float gainSum = 0;
    static float previousGainSum = 0;
    for(int i = 0; i < kNumBins;i++){
        float findex = static_cast<float>(i) * kWidthToBinRatio;
        float floor = std::floor(findex);
        float weight = findex - floor;
        int index = static_cast<int>(floor);
        if(index >= kKinectWidth-1){
            pdGainBuffer[i] = ofMap(gainContour[kKinectWidth-1].y, -1.0, 1.0, 0.0, 1.0, true);
        }else{
            float gainLeft = gainContour[index].y;
            float gainRight = gainContour[index+1].y;
            float gainVal = (gainRight-gainLeft) * weight + gainLeft;
            pdGainBuffer[i] = ofMap(gainVal, -1.0,1.0,0.0,1.0, true);
        }
        gainSum += pdGainBuffer[i];
    }
    Trigger status = gainSum > kEnterThreshold ? Trigger::Enter : Trigger::Exit;
    trigger = (previousStat == status) ? Trigger::Stay : status;
    previousStat = status;
    return gainSum / kNumBins;
}

void ofApp::update(){
    
    // gain contour
    float gainAvg = updateGainContour();
    pd.writeArray("gain", pdGainBuffer);
    gainContourVbo.updateVertexData(&gainContour[0], kKinectWidth);

    // trigger application
    if(trigger == Trigger::Enter){
        touchEffect.reset(0.0);
        touchEffect.setDuration(1.0);
        touchEffect.setCurve(EASE_OUT);
        touchEffect.animateTo(1.0);
    }else if(trigger == Trigger::Exit){
//        touchEffect.reset();
    }
    
    unsigned char fillAlpha = static_cast<unsigned char>(ofClamp(gainAvg * 1024.0, 0.0, 255.0));
    unsigned char frameAlpha = 255;
    
    scanner.update(fillAlpha , frameAlpha);
    // camera
    cameraAnimation.update(kCameraSpeed);
    lookAtAnimation.update(kCameraSpeed);
    ofPoint currentCamera, currentLookAt;
    if(guiEnabled){
        currentCamera = cameraPosSlider;
        currentLookAt = lookAtSlider;
    }else{
        currentCamera= cameraAnimation.getCurrentPosition();
        currentLookAt = lookAtAnimation.getCurrentPosition();
    }
    
    camera.setPosition(currentCamera);
    camera.lookAt(currentLookAt);
    
    // read spectrum
    pd.readArray("pastSpectrum", pdPastSpectrumBuffer);
    pd.readArray("futureSpectrum",pdFutureSpectrumBuffer);
    pastSpectrogram.update(pdPastSpectrumBuffer);
    futureSpectrogram.update(pdFutureSpectrumBuffer);
    
    // effect
    if(touchEffect.isAnimating()){
        touchEffect.update(0.02);
    }
    
    // read kinect data and sonificate
    kinect.update();
    if(kinect.isFrameNewDepth()){
        std::for_each(gainContour.begin(), gainContour.end(), [](ofPoint & point){
            point.y = -1.0;
        });
        pointCloud.update(kinect.getDepthPixels(),gainContour,distThresholdSlider);
    }
}

void ofApp::drawWorld(){

    camera.begin();

    if(boxEnabled){ ofNoFill();ofDrawBox(2,2,2);}
    pointCloud.draw();
    
    ofSetLineWidth(1);
    ofSetColor(ofColor::lightBlue);
    gainContourVbo.draw(GL_LINE_STRIP, 0, kKinectWidth );
    
    // spectrograms
    pastSpectrogram.draw();

    ofPushMatrix();
    ofTranslate(0,-2,0);
    ofRotateZ(180);
    futureSpectrogram.draw();
    ofPopMatrix();
    
    // scanner
    ofPushMatrix();
    ofRotateY(180);
    ofPopMatrix();
    scanner.draw();
    
    // effect
    if(touchEffect.isAnimating()){
        ofPushMatrix();
        float value = touchEffect.getCurrentValue();
        ofSetColor(ofFloatColor(0.6,0.9,0.9,1.0-value ));
        float scale = value* 10;
        ofScale(scale, scale);
        ofDrawRectangle(-1,-1, 2,2);
        ofPopMatrix();
    }
    
    camera.end();
}

void ofApp::drawGui(){
    ofSetColor(255, 255, 255, 255);
    ofDisableDepthTest();
    gui.draw();
    ofEnableDepthTest();
}

void ofApp::draw(){
    ofSetColor(ofColor::white);
    drawWorld();
    if(guiEnabled)drawGui();
}

void ofApp::exit(){
    kinect.setCameraTiltAngle(0); // zero the tilt on exit
    kinect.close();
}

void ofApp::audioReceived(float * input, int bufferSize, int nChannels) {
   pd.audioIn(input, bufferSize, nChannels);
}

void ofApp::audioRequested(float * output, int bufferSize, int nChannels) {
    pd.audioOut(output, bufferSize, nChannels);
}

void ofApp::keyPressed(int key){
    switch (key){
    case 'b':
        boxEnabled = !boxEnabled;
        break;
    case 'g':
        guiEnabled = !guiEnabled;
        break;
    case 't':
        pd.sendBang("testTone");
        break;
    }
}

