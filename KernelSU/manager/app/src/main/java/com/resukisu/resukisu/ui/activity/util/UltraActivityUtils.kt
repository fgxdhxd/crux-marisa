package com.resukisu.resukisu.ui.activity.util

import android.content.Context
import android.net.Uri
import androidx.core.content.edit
import androidx.lifecycle.lifecycleScope
import com.ramcosta.composedestinations.generated.destinations.FlashScreenDestination
import com.ramcosta.composedestinations.generated.destinations.InstallScreenDestination
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import com.resukisu.resukisu.Natives
import com.resukisu.resukisu.ui.MainActivity
import com.resukisu.resukisu.ui.component.ZipFileDetector
import com.resukisu.resukisu.ui.component.ZipFileInfo
import com.resukisu.resukisu.ui.component.ZipType
import com.resukisu.resukisu.ui.screen.FlashIt
import com.resukisu.resukisu.ui.util.getKpmVersion
import com.resukisu.resukisu.ui.util.rootAvailable
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

object UltraActivityUtils {

    suspend fun detectZipTypeAndShowConfirmation(
        activity: MainActivity,
        zipUris: ArrayList<Uri>,
        onResult: (List<ZipFileInfo>) -> Unit
    ) {
        val infos = ZipFileDetector.detectAndParseZipFiles(activity, zipUris)
        withContext(Dispatchers.Main) { onResult(infos) }
    }

    fun navigateToFlashScreen(
        activity: MainActivity,
        zipFiles: List<ZipFileInfo>,
        navigator: DestinationsNavigator
    ) {
        activity.lifecycleScope.launch {
            val moduleUris = zipFiles.filter { it.type == ZipType.MODULE }.map { it.uri }
            val kernelUris = zipFiles.filter { it.type == ZipType.KERNEL }.map { it.uri }

            when {
                kernelUris.isNotEmpty() && moduleUris.isEmpty() -> {
                    if (kernelUris.size == 1 && rootAvailable()) {
                        navigator.navigate(
                            InstallScreenDestination(
                                preselectedKernelUri = kernelUris.first().toString()
                            )
                        )
                    }
                    setAutoExitAfterFlash(activity)
                }

                moduleUris.isNotEmpty() -> {
                    navigator.navigate(
                        FlashScreenDestination(
                            FlashIt.FlashModules(ArrayList(moduleUris))
                        )
                    )
                    setAutoExitAfterFlash(activity)
                }
            }
        }
    }

    private fun setAutoExitAfterFlash(activity: Context) {
        activity.getSharedPreferences("kernel_flash_prefs", Context.MODE_PRIVATE)
            .edit {
                putBoolean("auto_exit_after_flash", true)
            }
    }
}

object AppData {
    /**
     * 获取KPM版本
     */
    fun getKpmVersionUse(): String {
        return try {
            if (!rootAvailable()) return ""
            val version = getKpmVersion()
            version.ifEmpty { "" }
        } catch (e: Exception) {
            "Error: ${e.message}"
        }
    }

    /**
     * 检查是否是完整功能模式
     */
    fun isFullFeatured(): Boolean {
        val isManager = Natives.isManager
        return isManager && !Natives.requireNewKernel() && rootAvailable()
    }
}