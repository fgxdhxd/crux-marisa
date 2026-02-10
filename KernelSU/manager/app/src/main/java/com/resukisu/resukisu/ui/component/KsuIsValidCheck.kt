package com.resukisu.resukisu.ui.component

import androidx.compose.runtime.Composable
import com.resukisu.resukisu.Natives

@Composable
fun KsuIsValid(
    content: @Composable () -> Unit
) {
    val isManager = Natives.isManager
    val ksuVersion = if (isManager) Natives.version else null

    if (ksuVersion != null) {
        content()
    }
}

/**
 * Check the manager is valid or not
 *
 * true = ksu valid
 * false = ksu invalid
 *
 * invalid = not is manager
 */
fun ksuIsValid() : Boolean {
    val isManager = Natives.isManager
    val ksuVersion = if (isManager) Natives.version else null
    return ksuVersion != null
}