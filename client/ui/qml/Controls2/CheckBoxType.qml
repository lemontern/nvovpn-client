import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

import Style 1.0

import "TextTypes"

CheckBox {
    id: root

    property string descriptionText
    property string descriptionTextColor: NvoStyle.color.mutedGray
    property string descriptionTextDisabledColor: NvoStyle.color.charcoalGray

    property string textColor: NvoStyle.color.paleGray
    property string textDisabledColor: NvoStyle.color.mutedGray

    property string hoveredColor: NvoStyle.color.barelyTranslucentWhite
    property string defaultColor: NvoStyle.color.transparent
    property string pressedColor: NvoStyle.color.barelyTranslucentWhite

    property string defaultBorderColor: NvoStyle.color.paleGray
    property string checkedBorderColor: NvoStyle.color.goldenApricot
    property string checkedBorderDisabledColor: NvoStyle.color.deepBrown

    property string borderFocusedColor: NvoStyle.color.paleGray

    property string checkedImageColor: NvoStyle.color.goldenApricot
    property string pressedImageColor: NvoStyle.color.burntOrange
    property string defaultImageColor: NvoStyle.color.transparent
    property string checkedDisabledImageColor: NvoStyle.color.mutedBrown

    property string imageSource: "qrc:/images/controls/check.svg"

    property bool isFocusable: true

    Keys.onTabPressed: {
        FocusController.nextKeyTabItem()
    }

    Keys.onBacktabPressed: {
        FocusController.previousKeyTabItem()
    }

    Keys.onUpPressed: {
        FocusController.nextKeyUpItem()
    }
    
    Keys.onDownPressed: {
        FocusController.nextKeyDownItem()
    }
    
    Keys.onLeftPressed: {
        FocusController.nextKeyLeftItem()
    }

    Keys.onRightPressed: {
        FocusController.nextKeyRightItem()
    }

    hoverEnabled: enabled ? true : false
    focusPolicy: Qt.NoFocus

    background: Rectangle {
        color: NvoStyle.color.transparent
        border.color: root.focus ? borderFocusedColor : NvoStyle.color.transparent
        border.width: 1
        radius: 16
    }

    indicator: Rectangle {
        id: background

        anchors.verticalCenter: parent.verticalCenter

        implicitWidth: 56
        implicitHeight: 56
        radius: 16

        color:  {
            if (root.hovered) {
                return hoveredColor
            }
            return defaultColor
        }

        Behavior on color {
            PropertyAnimation { duration: 200 }
        }

        Rectangle {
            id: imageBorder

            anchors.centerIn: parent
            width: 24
            height: 24
            color: NvoStyle.color.transparent
            border.color: root.checked ?
                              (root.enabled ?
                                   checkedBorderColor :
                                   checkedBorderDisabledColor) :
                              defaultBorderColor
            border.width: 1
            radius: 4

            Image {
                anchors.centerIn: parent

                source: root.pressed ? imageSource : root.checked ? imageSource : ""
                layer {
                    enabled: true
                    effect: ColorOverlay {
                        color: {
                            if (root.pressed) {
                                return root.pressedImageColor
                            } else if (root.checked) {
                                if (root.enabled) {
                                    return root.checkedImageColor
                                } else {
                                    return root.checkedDisabledImageColor
                                }
                            } else {
                                return root.defaultImageColor
                            }
                        }
                    }
                }
            }
        }
    }

    contentItem: Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 8 + background.width

        implicitHeight: content.implicitHeight

        ColumnLayout {
            id: content

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            spacing: 4

            ListItemTitleType {
                Layout.fillWidth: true

                text: root.text
                color: root.enabled ? root.textColor : root.textDisabledColor
            }

            CaptionTextType {
                id: description

                Layout.fillWidth: true

                text: root.descriptionText
                color: root.enabled ? root.descriptionTextColor : root.descriptionTextDisabledColor

                visible: root.descriptionText !== ""
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        enabled: false
    }

    Keys.onEnterPressed: event => handleSwitch(event)
    Keys.onReturnPressed: event => handleSwitch(event)
    Keys.onSpacePressed: event => handleSwitch(event)

    function handleSwitch(event) {
        if (!event.isAutoRepeat) {
            root.checked = !root.checked
            root.checkedChanged()
        }
        event.accepted = true
    }
}


