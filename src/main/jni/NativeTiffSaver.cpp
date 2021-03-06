//
// Created by beyka on 18.2.16.
//
using namespace std;

#ifdef __cplusplus
extern "C" {
    #endif

    #include "NativeTiffSaver.h"
    #include "NativeExceptions.h"

    int const colorMask = 0xFF;

    int const paramCompression = 0;
    int const paramOrientation = 1;

    JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffSaver_save
    (JNIEnv *env, jclass clazz, jstring filePath, jintArray img, jobject options, jint img_width, jint img_height, jboolean append) {

        const char *strPath = NULL;
        strPath = env->GetStringUTFChars(filePath, 0);
        LOGIS("nativeTiffOpenForSave", strPath);

        //Get array of jint from jintArray
        jint *c_array;
        c_array = env->GetIntArrayElements(img, NULL);
        if (c_array == NULL) {
            //if array is null - nothing to save
            LOGE("array is null");
            return JNI_FALSE;
        }

        //Get options
        jclass jSaveOptionsClass = env->FindClass(
        "org/beyka/tiffbitmapfactory/TiffSaver$SaveOptions");
        //Get compression mode from options object
        jfieldID gOptions_CompressionModeFieldID = env->GetFieldID(jSaveOptionsClass,
        "compressionScheme",
        "Lorg/beyka/tiffbitmapfactory/CompressionScheme;");
        jobject compressionMode = env->GetObjectField(options, gOptions_CompressionModeFieldID);

        jclass compressionModeClass = env->FindClass(
        "org/beyka/tiffbitmapfactory/CompressionScheme");
        jfieldID ordinalFieldID = env->GetFieldID(compressionModeClass, "ordinal", "I");
        jint compressionInt = env->GetIntField(compressionMode, ordinalFieldID);

        //Get image orientation from options object
        jfieldID gOptions_OrientationFieldID = env->GetFieldID(jSaveOptionsClass,
        "orientation",
        "Lorg/beyka/tiffbitmapfactory/Orientation;");
        jobject orientation = env->GetObjectField(options, gOptions_OrientationFieldID);

        jclass orientationClass = env->FindClass(
        "org/beyka/tiffbitmapfactory/Orientation");
        jfieldID orientationOrdinalFieldID = env->GetFieldID(orientationClass, "ordinal", "I");
        jint orientationInt = env->GetIntField(orientation, orientationOrdinalFieldID);

        //Get author field if exist
        jfieldID gOptions_authorFieldID = env->GetFieldID(jSaveOptionsClass, "author", "Ljava/lang/String;");
        jstring jAuthor = (jstring)env->GetObjectField(options, gOptions_authorFieldID);
        const char *authorString = NULL;
        if (jAuthor) {
            authorString = env->GetStringUTFChars(jAuthor, 0);
            LOGIS("Author: ", authorString);
        }

        //Get copyright field if exist
        jfieldID gOptions_copyrightFieldID = env->GetFieldID(jSaveOptionsClass, "copyright", "Ljava/lang/String;");
        jstring jCopyright = (jstring)env->GetObjectField(options, gOptions_copyrightFieldID);
        const char *copyrightString = NULL;
        if (jCopyright) {
            copyrightString = env->GetStringUTFChars(jCopyright, 0);
            LOGIS("Copyright: ", copyrightString);
        }

        //Get image description field if exist
        jfieldID gOptions_imgDescrFieldID = env->GetFieldID(jSaveOptionsClass, "imageDescription", "Ljava/lang/String;");
        jstring jImgDescr = (jstring)env->GetObjectField(options, gOptions_imgDescrFieldID);
        const char *imgDescrString = NULL;
        if (jImgDescr) {
            imgDescrString = env->GetStringUTFChars(jImgDescr, 0);
            LOGIS("Image Description: ", imgDescrString);
        }

        //Get software name and number from buildconfig
        jclass jBuildConfigClass = env->FindClass(
                "org/beyka/tiffbitmapfactory/BuildConfig");
        jfieldID softwareNameFieldID = env->GetStaticFieldID(jBuildConfigClass, "softwarename", "Ljava/lang/String;");
        jstring jsoftwarename = (jstring)env->GetStaticObjectField(jBuildConfigClass, softwareNameFieldID);
        const char *softwareNameString = NULL;
        if (jsoftwarename) {
            softwareNameString = env->GetStringUTFChars(jsoftwarename, 0);
            LOGIS("Software Name: ", softwareNameString);
        }

        //Get android version
        jclass build_class = env->FindClass("android/os/Build$VERSION");
        jfieldID releaseFieldID = env->GetStaticFieldID(build_class, "RELEASE", "Ljava/lang/String;");
        jstring jrelease = (jstring)env->GetStaticObjectField(build_class, releaseFieldID);
        const char *releaseString = NULL;
        if (jrelease) {
            releaseString = env->GetStringUTFChars(jrelease, 0);
            LOGIS("Release: ", releaseString);
        }
        char *fullReleaseName = concat("Android ", releaseString);
        //strcpy(fullReleaseName, android);
        //strcpy(fullReleaseName, releaseString);
        LOGIS("Full Release: ", fullReleaseName);

        int pixelsBufferSize = img_width * img_height;
        int* array = (int *) malloc(sizeof(int) * pixelsBufferSize);
        if (!array) {
            throw_not_enought_memory_exception(env, sizeof(int) * pixelsBufferSize);
            return JNI_FALSE;
        }
        for (int i = 0; i < img_width; i++) {
            for (int j = 0; j < img_height; j++) {
                jint crPix = c_array[j * img_width + i];
                int alpha = colorMask & crPix >> 24;
                int red = colorMask & crPix >> 16;
                int green = colorMask & crPix >> 8;
                int blue = colorMask & crPix;

                crPix = (alpha << 24) | (blue << 16) | (green << 8) | (red);
                array[j * img_width + i] = crPix;
            }
        }

        TIFF *output_image;
        int fileDescriptor = -1;

        // Open the TIFF file
        if (!append) {
            if((output_image = TIFFOpen(strPath, "w")) == NULL){
                LOGE("can not open file. Trying file descriptor");
                //if TIFFOpen returns null then try to open file from descriptor
                int mode = O_RDWR | O_CREAT | O_TRUNC | 0;
                fileDescriptor = open(strPath, mode, 0666);
                if (fileDescriptor < 0) {
                    LOGE("Unable to create tif file descriptor");
                    throw_no_such_file_exception(env, filePath);
                    return JNI_FALSE;
                } else {
                    if ((output_image = TIFFFdOpen(fileDescriptor, strPath, "w")) == NULL) {
                        close(fileDescriptor);
                        LOGE("Unable to write tif file");
                        throw_no_such_file_exception(env, filePath);
                        return JNI_FALSE;
                    }
                }
            }
        } else {
            if((output_image = TIFFOpen(strPath, "a")) == NULL){
                LOGE("can not open file. Trying file descriptor");
                //if TIFFOpen returns null then try to open file from descriptor
                int mode = O_RDWR|O_CREAT;
                fileDescriptor = open(strPath, mode, 0666);
                if (fileDescriptor < 0) {
                    LOGE("Unable to create tif file descriptor");
                    throw_no_such_file_exception(env, filePath);
                    return JNI_FALSE;
                } else {
                    if ((output_image = TIFFFdOpen(fileDescriptor, strPath, "a")) == NULL) {
                        close(fileDescriptor);
                        LOGE("Unable to write tif file");
                        throw_no_such_file_exception(env, filePath);
                        return JNI_FALSE;
                    }
                }
            }
        }

        //Write tiff tags for saveing
        TIFFSetField(output_image, TIFFTAG_IMAGEWIDTH, img_width);
        TIFFSetField(output_image, TIFFTAG_IMAGELENGTH, img_height);
        TIFFSetField(output_image, TIFFTAG_BITSPERSAMPLE, 8);
        TIFFSetField(output_image, TIFFTAG_SAMPLESPERPIXEL, 4);
        TIFFSetField(output_image, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(output_image, TIFFTAG_COMPRESSION, compressionInt);
        TIFFSetField(output_image, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        TIFFSetField(output_image, TIFFTAG_ORIENTATION, orientationInt);

        //Write additiona tags
        //CreationDate tag
        char *date = getCreationDate();
        TIFFSetField(output_image, TIFFTAG_DATETIME, date);
        free(date);
        //Host system
        TIFFSetField(output_image, TIFFTAG_HOSTCOMPUTER, fullReleaseName);

        //software
        if (softwareNameString) {
            TIFFSetField(output_image, TIFFTAG_SOFTWARE, softwareNameString);
        }
        //image description
        if (imgDescrString) {
            TIFFSetField(output_image, TIFFTAG_IMAGEDESCRIPTION, imgDescrString);
        }
        //author
        if (authorString) {
            TIFFSetField(output_image, TIFFTAG_ARTIST, authorString);
        }
        //copyright
        if (copyrightString) {
            TIFFSetField(output_image, TIFFTAG_COPYRIGHT, copyrightString);
        }

        // Write the information to the file
        for (int row = 0; row < img_height; row++) {
            TIFFWriteScanline(output_image, &array[row * img_width], row, 0);
        }
        int ret = TIFFWriteDirectory(output_image);
        LOGII("ret = ", ret);

        // Close the file
        TIFFClose(output_image);

        //if file descriptor was openned then close it

        if (fileDescriptor >= 0) {
            close(fileDescriptor);
        }

        //free temp array
        free (array);
        //Remove variables
        if (releaseString) {
            env->ReleaseStringUTFChars(jrelease, releaseString);
        }
        free(fullReleaseName);
        if (softwareNameString) {
            env->ReleaseStringUTFChars(jsoftwarename, softwareNameString);
        }
        if (imgDescrString) {
            env->ReleaseStringUTFChars(jImgDescr, imgDescrString);
        }
        if (authorString) {
            env->ReleaseStringUTFChars(jAuthor, authorString);
        }
        if (copyrightString) {
            env->ReleaseStringUTFChars(jCopyright, copyrightString);
        }
        env->ReleaseStringUTFChars(filePath, strPath);
        env->ReleaseIntArrayElements(img, c_array, 0);

        if (ret == -1) return JNI_FALSE;
        return JNI_TRUE;
    }

    char *getCreationDate() {
        char * datestr = (char *) malloc(sizeof(char) * 20);
        time_t rawtime;
        struct tm * timeinfo;
        time (&rawtime);
        timeinfo = localtime (&rawtime);
        strftime (datestr,20,/*"Now it's %I:%M%p."*/"%Y:%m:%d %H:%M:%S",timeinfo);

        return datestr;
    }

    char* concat(const char *s1, const char *s2)
    {
        char *result = (char *)malloc(strlen(s1)+strlen(s2)+1);//+1 for the zero-terminator
        //in real code you would check for errors in malloc here
        strcpy(result, s1);
        strcat(result, s2);
        return result;
    }

    #ifdef __cplusplus
}
#endif