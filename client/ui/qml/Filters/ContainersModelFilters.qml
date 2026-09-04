pragma Singleton

import QtQuick 2.15

import SortFilterProxyModel 0.2

Item {
    ValueFilter {
        id: vpnTypeFilter
        roleName: "isVpnContainer"
        value: true
    }

    ValueFilter {
        id: installedFilter
        roleName: "isInstalled"
        value: true
    }

    // Configs are imported, never installed from here, so only the installed
    // set is ever shown.
    function getInstalledProtocolsListFilters() {
        return [vpnTypeFilter, installedFilter]
    }
}
