#include "svmCalibFisheye.hpp"
#include "svmError.hpp"

sanCalibFisheye::sanCalibFisheye(sanXML* pxml)
{
	m_pxml = pxml;
}

sanCalibFisheye::~sanCalibFisheye()
{
    for (int i = 0; i < (int)m_fisheye_images.size(); i++)
		m_fisheye_images[i].release();

    if (!m_fisheye_images.empty()) m_fisheye_images.clear(); else noop;
    vector<Mat>().swap(m_fisheye_images);

    for (int i = 0; i < (int)m_additional_fisheye_images.size(); i++)
		m_additional_fisheye_images[i].release();

    if (!m_additional_fisheye_images.empty()) m_additional_fisheye_images.clear(); else noop;
    vector<Mat>().swap(m_additional_fisheye_images);

    m_fisheyeShader.~sanShader();
}

void sanCalibFisheye::initialize(unsigned char* img_front=NULL, unsigned char* img_right=NULL, unsigned char* img_rear=NULL, unsigned char* img_left=NULL, unsigned char* add_0=NULL)
{
    int img_width = (int)m_pxml->m_resolution.image.width;
    int img_height = (int)m_pxml->m_resolution.image.height;

    m_fisheye_images.push_back(Mat(img_height, img_width, CV_8UC3, img_front));
    m_fisheye_images.push_back(Mat(img_height, img_width, CV_8UC3, img_right));
    m_fisheye_images.push_back(Mat(img_height, img_width, CV_8UC3, img_rear));
    m_fisheye_images.push_back(Mat(img_height, img_width, CV_8UC3, img_left));
    m_additional_fisheye_images.push_back(Mat(img_height, img_width, CV_8UC3, add_0));

    for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
    {
        string str_camID = "(camID[" + to_string(camID) + "])";
        try
        {
            generateVAB(m_fisheyeLUT, 6, 3, 2, GL_STATIC_DRAW);
            sanVABT::generateTexture(GL_TEXTURE0, &m_vabt_list[getVABTLastIndex()].texID);
            sanVABT::updateTexture(GL_TEXTURE0, m_vabt_list[getVABTLastIndex()].texID, m_fisheye_images[camID].ptr(), img_width, img_height, m_fisheye_images[camID].channels(), BINDING);
        }
        catch (exception& e)
        {
            throw logger.svm_fatal("C1102001", __FUNCTION__ + str_camID + delimiter(string(e.what())));
        }            
    }    
}


void sanCalibFisheye::renderFisheyes(int camID)
{   
    try
    {
        m_fisheyeShader.use();
        glDisable(GL_BLEND);

        string str_camID = "(camID[" + to_string(camID) + "])";
        sanError::glClearError();

        glBindVertexArray(getVaoID(camID));
        glBindBuffer(GL_ARRAY_BUFFER, getVboID(camID));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, getTexID(camID));
        m_fisheyeShader.setInt("src_img", 0);
        glDrawArrays(GL_TRIANGLES, 0, getVnum(camID));

        glBindVertexArray(0);

        sanError::glCheckError(str_camID);
    }
    catch (exception& e)
    {
        throw logger.svm_fatal("C1202001", __FUNCTION__ + string(e.what()));
    }  
}

