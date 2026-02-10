package com.resukisu.resukisu.ui.screen

import android.content.Context
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.add
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.DeleteForever
import androidx.compose.material.icons.filled.Folder
import androidx.compose.material.icons.filled.Info
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExperimentalMaterial3ExpressiveApi
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LargeFlexibleTopAppBar
import androidx.compose.material3.LoadingIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.pulltorefresh.PullToRefreshBox
import androidx.compose.material3.pulltorefresh.PullToRefreshDefaults
import androidx.compose.material3.pulltorefresh.rememberPullToRefreshState
import androidx.compose.material3.rememberTopAppBarState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import com.ramcosta.composedestinations.annotation.Destination
import com.ramcosta.composedestinations.annotation.RootGraph
import com.ramcosta.composedestinations.navigation.DestinationsNavigator
import com.resukisu.resukisu.R
import com.resukisu.resukisu.ui.component.ConfirmResult
import com.resukisu.resukisu.ui.component.rememberConfirmDialog
import com.resukisu.resukisu.ui.component.settings.AppBackButton
import com.resukisu.resukisu.ui.theme.ThemeConfig
import com.resukisu.resukisu.ui.theme.getCardColors
import com.resukisu.resukisu.ui.theme.getCardElevation
import com.resukisu.resukisu.ui.util.LocalSnackbarHost
import com.resukisu.resukisu.ui.util.addUmountPath
import com.resukisu.resukisu.ui.util.applyUmountConfigToKernel
import com.resukisu.resukisu.ui.util.clearCustomUmountPaths
import com.resukisu.resukisu.ui.util.listUmountPaths
import com.resukisu.resukisu.ui.util.removeUmountPath
import com.resukisu.resukisu.ui.util.saveUmountConfig
import dev.chrisbanes.haze.HazeStyle
import dev.chrisbanes.haze.HazeTint
import dev.chrisbanes.haze.hazeEffect
import dev.chrisbanes.haze.hazeSource
import dev.chrisbanes.haze.rememberHazeState
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

private val SPACING_SMALL = 3.dp
private val SPACING_MEDIUM = 8.dp
private val SPACING_LARGE = 16.dp

data class UmountPathEntry(
    val path: String,
    val flags: Int,
)

@OptIn(ExperimentalMaterial3Api::class, ExperimentalMaterial3ExpressiveApi::class)
@Destination<RootGraph>
@Composable
fun UmountManagerScreen(navigator: DestinationsNavigator) {
    val scrollBehavior = TopAppBarDefaults.exitUntilCollapsedScrollBehavior(rememberTopAppBarState())
    val snackBarHost = LocalSnackbarHost.current
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val confirmDialog = rememberConfirmDialog()

    var pathList by remember { mutableStateOf<List<UmountPathEntry>>(emptyList()) }
    var isLoading by remember { mutableStateOf(false) }
    val pullToRefreshState = rememberPullToRefreshState()
    var isRefreshing by remember { mutableStateOf(false) }
    var showAddDialog by remember { mutableStateOf(false) }
    val hazeState = if (ThemeConfig.backgroundImageLoaded) rememberHazeState() else null

    val hazeStyle = if (ThemeConfig.backgroundImageLoaded) HazeStyle(
        backgroundColor = MaterialTheme.colorScheme.surfaceContainerHigh.copy(
            alpha = 0.8f
        ),
        tint = HazeTint(Color.Transparent)
    ) else null

    val collapsedFraction = scrollBehavior.state.collapsedFraction
    val modifier = if (ThemeConfig.backgroundImageLoaded && hazeStyle != null && hazeState != null) {
        Modifier.hazeEffect(hazeState) {
            style = hazeStyle
            noiseFactor = 0f
            blurRadius = 30.dp
            alpha = collapsedFraction
        }
    }
    else Modifier

    fun loadPaths() {
        scope.launch(Dispatchers.IO) {
            val result = listUmountPaths()
            val entries = parseUmountPaths(result)
            withContext(Dispatchers.Main) {
                pathList = entries
                isLoading = false
            }
        }
    }

    LaunchedEffect(Unit) {
        scrollBehavior.state.heightOffset =
            scrollBehavior.state.heightOffsetLimit
        loadPaths()
    }

    Scaffold(
        topBar = {
            LargeFlexibleTopAppBar(
                modifier = modifier,
                title = { Text(stringResource(R.string.umount_path_manager)) },
                navigationIcon = {
                    AppBackButton(
                        onClick = {
                            navigator.popBackStack()
                        }
                    )
                },
                windowInsets = TopAppBarDefaults.windowInsets.add(WindowInsets(left = 12.dp)),
                scrollBehavior = scrollBehavior,
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor =
                        if (ThemeConfig.backgroundImageLoaded) Color.Transparent
                        else MaterialTheme.colorScheme.surfaceContainer,
                    scrolledContainerColor =
                        if (ThemeConfig.backgroundImageLoaded) Color.Transparent
                        else MaterialTheme.colorScheme.surfaceContainer,
                )
            )
        },
        floatingActionButton = {
            FloatingActionButton(
                onClick = { showAddDialog = true }
            ) {
                Icon(Icons.Filled.Add, contentDescription = null)
            }
        },
        snackbarHost = { SnackbarHost(snackBarHost) },
        containerColor = Color.Transparent,
        contentColor = MaterialTheme.colorScheme.onSurface,
    ) { paddingValues ->
        if (isLoading) { // 初次加载时动画
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .nestedScroll(scrollBehavior.nestedScrollConnection),
                contentAlignment = Alignment.Center,
            ) {
                LoadingIndicator()
            }
        }
        else {
            PullToRefreshBox(
                state = pullToRefreshState,
                isRefreshing = isRefreshing,
                onRefresh = {
                    loadPaths()
                    isRefreshing = false
                },
                indicator = {
                    PullToRefreshDefaults.LoadingIndicator(
                        state = pullToRefreshState,
                        isRefreshing = isRefreshing,
                        modifier = Modifier
                            .padding(top = paddingValues.calculateTopPadding())
                            .align(Alignment.TopCenter),
                    )
                },
                modifier = if (hazeState != null) Modifier.hazeSource(hazeState) else Modifier
            ) {
                LazyColumn(
                    modifier = Modifier
                        .fillMaxSize()
                        .nestedScroll(scrollBehavior.nestedScrollConnection),
                    contentPadding = PaddingValues(
                        horizontal = SPACING_LARGE,
                        vertical = SPACING_MEDIUM
                    ),
                    verticalArrangement = Arrangement.spacedBy(SPACING_MEDIUM)
                ) {
                    item {
                        Spacer(modifier = Modifier.height(paddingValues.calculateTopPadding()))
                    }

                    item {
                        Card(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(horizontal = 10.dp),
                            colors = getCardColors(MaterialTheme.colorScheme.primaryContainer),
                            elevation = getCardElevation()
                        ) {
                            Row(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .padding(5.dp),
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Icon(
                                    imageVector = Icons.Filled.Info,
                                    contentDescription = null,
                                    modifier = Modifier.padding(10.dp),
                                    tint = MaterialTheme.colorScheme.onPrimaryContainer
                                )
                                Text(
                                    text = stringResource(R.string.umount_path_restart_notice),
                                    style = MaterialTheme.typography.bodyMedium,
                                    color = MaterialTheme.colorScheme.onPrimaryContainer
                                )
                            }
                        }
                    }

                    if (pathList.isEmpty()) {
                        item {
                            Spacer(modifier = Modifier.height(15.dp))
                            Card(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .padding(horizontal = 10.dp),
                                elevation = getCardElevation()
                            ) {
                                Row(
                                    modifier = Modifier
                                        .fillMaxWidth()
                                        .padding(5.dp),
                                    verticalAlignment = Alignment.CenterVertically
                                ) {
                                    Icon(
                                        imageVector = Icons.Filled.Info,
                                        contentDescription = null,
                                        modifier = Modifier.padding(10.dp),
                                        tint = MaterialTheme.colorScheme.onSecondaryContainer
                                    )
                                    Text(
                                        text = stringResource(R.string.no_any_umount_path),
                                        style = MaterialTheme.typography.bodyMedium,
                                        color = MaterialTheme.colorScheme.onSecondaryContainer
                                    )
                                }
                            }
                        }
                    }

                    items(pathList, key = { it.path }) { entry ->
                        UmountPathCard(
                            entry = entry,
                            onDelete = {
                                scope.launch(Dispatchers.IO) {
                                    val success = removeUmountPath(entry.path)
                                    withContext(Dispatchers.Main) {
                                        if (success) {
                                            snackBarHost.showSnackbar(
                                                context.getString(R.string.umount_path_removed)
                                            )
                                            loadPaths()
                                        } else {
                                            snackBarHost.showSnackbar(
                                                context.getString(R.string.operation_failed)
                                            )
                                        }
                                    }
                                }
                            }
                        )
                    }

                    item {
                        Spacer(modifier = Modifier.height(SPACING_LARGE))
                    }

                    item {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(horizontal = SPACING_LARGE),
                            horizontalArrangement = Arrangement.spacedBy(SPACING_MEDIUM)
                        ) {
                            Button(
                                onClick = {
                                    scope.launch {
                                        if (confirmDialog.awaitConfirm(
                                                title = context.getString(R.string.confirm_action),
                                                content = context.getString(R.string.confirm_clear_custom_paths)
                                            ) == ConfirmResult.Confirmed
                                        ) {
                                            withContext(Dispatchers.IO) {
                                                val success = clearCustomUmountPaths()
                                                withContext(Dispatchers.Main) {
                                                    if (success) {
                                                        snackBarHost.showSnackbar(
                                                            context.getString(R.string.custom_paths_cleared)
                                                        )
                                                        loadPaths()
                                                    } else {
                                                        snackBarHost.showSnackbar(
                                                            context.getString(R.string.operation_failed)
                                                        )
                                                    }
                                                }
                                            }
                                        }
                                    }
                                },
                                modifier = Modifier.weight(1f)
                            ) {
                                Icon(Icons.Filled.DeleteForever, contentDescription = null)
                                Spacer(modifier = Modifier.width(SPACING_MEDIUM))
                                Text(stringResource(R.string.clear_custom_paths))
                            }

                            Button(
                                onClick = {
                                    scope.launch(Dispatchers.IO) {
                                        val success = applyUmountConfigToKernel()
                                        withContext(Dispatchers.Main) {
                                            if (success) {
                                                snackBarHost.showSnackbar(
                                                    context.getString(R.string.config_applied)
                                                )
                                            } else {
                                                snackBarHost.showSnackbar(
                                                    context.getString(R.string.operation_failed)
                                                )
                                            }
                                        }
                                    }
                                },
                                modifier = Modifier.weight(1f)
                            ) {
                                Icon(Icons.Filled.Check, contentDescription = null)
                                Spacer(modifier = Modifier.width(SPACING_MEDIUM))
                                Text(stringResource(R.string.apply_config))
                            }
                        }
                    }
                }
            }
        }

        if (showAddDialog) {
            AddUmountPathDialog(
                onDismiss = { showAddDialog = false },
                onConfirm = { path, flags ->
                    showAddDialog = false

                    scope.launch(Dispatchers.IO) {
                        val success = addUmountPath(path, flags)
                        withContext(Dispatchers.Main) {
                            if (success) {
                                saveUmountConfig()
                                snackBarHost.showSnackbar(
                                    context.getString(R.string.umount_path_added)
                                )
                                loadPaths()
                            } else {
                                snackBarHost.showSnackbar(
                                    context.getString(R.string.operation_failed)
                                )
                            }
                        }
                    }
                }
            )
        }
    }
}

@Composable
fun UmountPathCard(
    entry: UmountPathEntry,
    onDelete: () -> Unit
) {
    val confirmDialog = rememberConfirmDialog()
    val scope = rememberCoroutineScope()
    val context = LocalContext.current

    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = getCardColors(MaterialTheme.colorScheme.surfaceContainerLow),
        elevation = getCardElevation()
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(SPACING_LARGE),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Icon(
                imageVector = Icons.Filled.Folder,
                contentDescription = null,
                tint = MaterialTheme.colorScheme.primary,
                modifier = Modifier.size(24.dp)
            )

            Spacer(modifier = Modifier.width(SPACING_LARGE))

            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = entry.path,
                    style = MaterialTheme.typography.titleMedium
                )
                Spacer(modifier = Modifier.height(SPACING_SMALL))
                Text(
                    text = buildString {
                        append(context.getString(R.string.flags))
                        append(": ")
                        append(entry.flags.toUmountFlagName(context))
                    },
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
            IconButton(
                onClick = {
                    scope.launch {
                        if (confirmDialog.awaitConfirm(
                                title = context.getString(R.string.confirm_delete),
                                content = context.getString(R.string.confirm_delete_umount_path, entry.path)
                            ) == ConfirmResult.Confirmed) {
                            onDelete()
                        }
                    }
                }
            ) {
                Icon(
                    imageVector = Icons.Filled.Delete,
                    contentDescription = null,
                    tint = MaterialTheme.colorScheme.error
                )
            }
        }
    }
}

@Composable
fun AddUmountPathDialog(
    onDismiss: () -> Unit,
    onConfirm: (String, Int) -> Unit
) {
    var path by rememberSaveable { mutableStateOf("") }
    var flags by rememberSaveable { mutableStateOf("0") }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.add_umount_path)) },
        text = {
            Column {
                OutlinedTextField(
                    value = path,
                    onValueChange = { path = it },
                    label = { Text(stringResource(R.string.mount_path)) },
                    modifier = Modifier.fillMaxWidth(),
                    singleLine = true
                )

                Spacer(modifier = Modifier.height(SPACING_MEDIUM))

                OutlinedTextField(
                    value = flags,
                    onValueChange = { flags = it },
                    label = { Text(stringResource(R.string.umount_flags)) },
                    modifier = Modifier.fillMaxWidth(),
                    singleLine = true,
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                    supportingText = { Text(stringResource(R.string.umount_flags_hint)) }
                )
            }
        },
        confirmButton = {
            TextButton(
                onClick = {
                    val flagsInt = flags.toIntOrNull() ?: 0
                    onConfirm(path, flagsInt)
                },
                enabled = path.isNotBlank()
            ) {
                Text(stringResource(android.R.string.ok))
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text(stringResource(android.R.string.cancel))
            }
        }
    )
}

private fun parseUmountPaths(output: String): List<UmountPathEntry> {
    val lines = output.lines().filter { it.isNotBlank() }
    if (lines.size < 2) return emptyList()

    return lines.drop(2).mapNotNull { line ->
        val parts = line.trim().split(Regex("\\s+"))
        if (parts.size >= 2) {
            UmountPathEntry(
                path = parts[0],
                flags = parts[1].toIntOrNull() ?: 0
            )
        } else null
    }
}

private fun Int.toUmountFlagName(context: Context): String {
    return when (this) {
        2 -> context.getString(R.string.mnt_detach)
        else -> this.toString()
    }
}
