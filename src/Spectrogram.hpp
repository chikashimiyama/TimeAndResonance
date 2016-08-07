#pragma once

#include "const.hpp"
#include "ofMain.h"

class Spectrogram{
public:
    void setup(bool rev = false);
    void update(const std::vector<float> &pdSpectrumBuffer);
    void draw();

protected:
    int recordHead;

    std::vector<ofPoint> spectrogramVertices;
    ofVbo spectrogramVbo;
    bool reverse;
};

inline void Spectrogram::setup(bool rev){
    reverse = rev;
    spectrogramVertices.reserve(kNumVertices);
    
    for(int i = 0; i < kNumTimeSlices; i++){
        float alpha = rev ? kNumTimeSlices * i :(1.0 - kRNumTimeSlices * i);
        
        for(int j = 0; j < kNumBins; j++){
            float phase = static_cast<float>(j) / static_cast<float>(kNumBins);
            spectrogramVertices.emplace_back(2.0 * phase - 1.0,-1.0,0);
        }
    }
    spectrogramVbo.setVertexData(&spectrogramVertices[0],kNumVertices ,GL_DYNAMIC_DRAW);

}

inline void Spectrogram::update(const std::vector<float> &pdSpectrumBuffer){
    
    recordHead++;
    recordHead %= kNumTimeSlices;

    
    int pixelOffset = recordHead * kNumBins;

    for(int i = 0; i < kNumBins ;i++){
        float findex = static_cast<float>(i) * kWidthToBinRatio;
        float floor = std::floor(findex);
        float weight = findex - floor;
        int index = static_cast<int>(floor);
        spectrogramVertices[pixelOffset+i].y = pdSpectrumBuffer[i] - 1.0;
    }
    spectrogramVbo.updateVertexData(&spectrogramVertices[0], kNumVertices );

}

inline void Spectrogram::draw(){
    ofSetLineWidth(1);
    
    float maxDistance = kDistanceBetweenLines * (kNumTimeSlices-1);
    float maxSpread = kLineSpread* (kNumTimeSlices-1);

    float step = 0.0;
    for(int i = 0; i < kNumTimeSlices;i++){

        float distance = kDistanceBetweenLines * i;
        float scale = 1 + kLineSpread  * i;
        if(reverse) distance = maxDistance - distance;
        if(reverse) scale = 1 + maxSpread - kLineSpread * i;
        
        float alpha = reverse ? step : 1.0 - step;
        ofPushMatrix();
        ofSetColor(ofFloatColor(1.0,1.0,1.0, alpha));
        glTranslatef(0,0,distance );
        glScalef(scale, 1, scale);
        int readHead = recordHead -i;
        if(readHead < 0) readHead += kNumTimeSlices;
        int offset = readHead * kNumBins;
        spectrogramVbo.draw(GL_LINE_STRIP, offset, kNumBins);
        ofPopMatrix();
        step += kRNumTimeSlices/2;
    }

}
