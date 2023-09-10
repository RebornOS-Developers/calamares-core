/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2021 Anke Boersma <demm@kaosx.us>
 *   SPDX-FileCopyrightText: 2021 shiva.patt <shivanandvp@rebornos.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

import io.calamares.core 1.0
import io.calamares.ui 1.0

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 1.4 as OldControls

Item {
    anchors.fill:parent
    OldControls.TreeView {
        anchors.fill: parent
        OldControls.TableViewColumn {
            title: "Name"
            role: "display"
            width: 100
        }
        OldControls.TableViewColumn {
            title: "Description"
            role: "display"
            width: 300
        }
        model: config.packageModel
    }
}
