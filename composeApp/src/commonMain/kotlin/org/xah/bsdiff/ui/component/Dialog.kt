package org.xah.bsdiff.ui.component

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.VerticalDivider
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog

@Composable
fun MyDialog(
    onDismissRequest: () -> Unit,
    onConfirmation: () -> Unit,
    dialogTitle: String = "提示",
    dialogText: String,
    conformText : String = "确定",
    dismissText : String = "取消",
) {
    Dialog(
        onDismissRequest = onDismissRequest,
    ) {
        Surface(
            shape = MaterialTheme.shapes.large,
            color = MaterialTheme.colorScheme.surface.copy(0.95f),
            modifier = Modifier.padding(appHorizontalDp)
        ) {
            Column {
                Column(
                    modifier = Modifier.padding(22.dp),
                    verticalArrangement = Arrangement.spacedBy(appHorizontalDp)
                ) {
                    Text(
                        text = dialogTitle,
                        style = MaterialTheme.typography.titleLarge,
                        color = MaterialTheme.colorScheme.onSurface
                    )

                    Text(
                        text = dialogText,
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )


                }
                HorizontalDivider(thickness = 0.7.dp)
                Row(
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    TextButton (
                        onClick = onDismissRequest,
                        shape = RoundedCornerShape(0.dp),
                        modifier = Modifier.fillMaxWidth().weight(.5f)
                    ) {
                        Text(text = dismissText)
                    }
                    VerticalDivider(thickness = 0.7.dp, modifier = Modifier.height(48.dp))
                    TextButton (
                        colors = ButtonDefaults.textButtonColors(contentColor = MaterialTheme.colorScheme.error),
                        onClick = onConfirmation,
                        shape = RoundedCornerShape(0.dp),
                        modifier = Modifier.fillMaxWidth().weight(.5f)
                    ) {
                        Text(text = conformText)
                    }
                }
            }
        }
    }
}