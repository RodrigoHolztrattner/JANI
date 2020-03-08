////////////////////////////////////////////////////////////////////////////////
// Filename: Inspector.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniConfig.h"
#include "imgui.h"
#include "imgui_internal.h"

///////////////
// NAMESPACE //
///////////////

// ImGui
JaniNamespaceBegin(ImGui)

static ImVec2 operator+(ImVec2 _first, ImVec2 _second)
{
    return ImVec2(_first.x + _second.x, _first.y + _second.y);
}

static ImVec2 operator-(ImVec2 _first, ImVec2 _second)
{
    return ImVec2(_first.x - _second.x, _first.y - _second.y);
}

static ImVec2 operator/(ImVec2 _first, float _value)
{
    return ImVec2(_first.x / _value, _first.y / _value);
}

static ImVec2 operator*(ImVec2 _first, float _value)
{
    return ImVec2(_first.x * _value, _first.y * _value);
}

static bool operator ==(const ImVec2& a, const ImVec2& b)
{
    return (a.x == b.x && a.y == b.y);
}

IMGUI_API static bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f)
{
    using namespace ImGui;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiID id = window->GetID("##Splitter");
    ImRect bb;
    bb.Min = window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
    bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
    return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 0.0f);
}

/*
* Draws a button that support a disabled parameter (ImGuiItemFlags_Disabled and alpha to 0.5f)
*/
IMGUI_API static bool Button(const char* label, const ImVec2& size_arg, bool disabled)
{
    if (disabled)
    {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }

    bool button_result = ImGui::Button(label, size_arg);

    if (disabled)
    {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
    }

    return button_result && !disabled;
}

/*
* Draws a colored collapsing header
*/
IMGUI_API static bool CollapsingHeaderColored(const char* _label, ImVec4 _color, ImGuiTreeNodeFlags _flags = 0)
{
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, _color);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, _color);
    ImGui::PushStyleColor(ImGuiCol_Header, _color);


    bool tree_node_result = ImGui::CollapsingHeader(
        _label,
        _flags
        | ImGuiTreeNodeFlags_DefaultOpen
        | ImGuiTreeNodeFlags_Selected);

    ImGui::PopStyleColor(3);

    return tree_node_result;
}

/*
* Begin a colored collapsing header that support drawing a background color for internal items
* Only call the EndCollapsingHeaderColored() if this function returns true
*/
IMGUI_API static bool BeginCollapsingHeaderColored(const char* _label, ImVec4 _header_color, ImGuiTreeNodeFlags _flags = 0)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    bool collapsing_header_result = ImGui::CollapsingHeaderColored(_label, _header_color);
    ImGui::PopStyleVar();

    if (collapsing_header_result)
    {
        ImGui::GetWindowDrawList()->ChannelsSplit(2);
        ImGui::GetWindowDrawList()->ChannelsSetCurrent(1);

        float width_padding = ImGui::GetStyle().ItemSpacing.x;
        float height_padding = ImGui::GetStyle().ItemSpacing.y;

        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x + width_padding, ImGui::GetCursorScreenPos().y + height_padding));
        ImGui::BeginGroup();
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y - height_padding));

        return true;
    }

    return false;
}

/*
* End a colored collapsing header, by default it will use the frame background color but any color can be specified
*/
IMGUI_API static void EndCollapsingHeaderColored(ImVec4 _background_color = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg))
{
    ImGui::EndGroup();

    float width_padding = ImGui::GetStyle().ItemSpacing.x;
    float height_padding = ImGui::GetStyle().ItemSpacing.y;
    ImVec2 group_size = ImGui::GetItemRectSize();
    ImVec2 initial_pos = ImGui::GetCursorScreenPos() - ImVec2(0.0f, group_size.y + height_padding * 3.0f + 2.0f);
    ImVec2 final_pos = ImVec2(ImGui::GetContentRegionMaxScreen().x - width_padding, ImGui::GetCursorScreenPos().y);

    ImGui::GetWindowDrawList()->ChannelsSetCurrent(0);
    ImGui::GetWindowDrawList()->AddRectFilled(initial_pos, final_pos, ImColor(_background_color), 5.0f, ImDrawCornerFlags_Bot);
    ImGui::Dummy(ImVec2(0.0f, height_padding));
    ImGui::GetWindowDrawList()->ChannelsMerge();
}


// ImGui
JaniNamespaceEnd(ImGui)