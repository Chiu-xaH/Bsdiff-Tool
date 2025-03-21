package org.xah.bsdiff.logic.util

import java.io.File

class BsdiffJNI {
    companion object {
        init {
            System.loadLibrary("bsdiff")
        }
    }

    external fun patch(oldFilePath: String, newFilePath: String, patchFilePath: String) : Int

    external fun merge(oldFilePath: String, patchFilePath: String, newFilePath: String) : Int

    external fun pickFile() : String?
}