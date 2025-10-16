#include <windows.h>
#include <cstdio>
#include <shellapi.h>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <sstream>

// UI 缩放因子（将所有尺寸放大 25%）
const float UI_SCALE = 1.25f;
static inline int S(int v) { return (int)(v * UI_SCALE + 0.5f); }

// 教学窗口尺寸 - 紧凑型触摸屏操作（已缩放）
const int WINDOW_WIDTH = S(120);   // 紧凑宽度
const int WINDOW_HEIGHT = S(80);   // 紧凑高度

// 区域高度定义
const int TITLE_HEIGHT = S(16);    // 标题区域高度（已缩放）
const int CLOSE_HEIGHT = S(10);    // 关闭区域高度  
const int BUTTON_AREA_HEIGHT = WINDOW_HEIGHT - TITLE_HEIGHT - CLOSE_HEIGHT; // 按钮区域高度

// 全局变量
HWND g_hwnd;
bool g_isDragging = false;
POINT g_dragOffset;

// 配置
std::vector<std::wstring> g_configStudents; // if non-empty, use these for random
std::wstring g_innerUrl; // if set, use this as the URL to open (for button 0)
std::wstring g_manualIp; // keep manual ip if present (for button 0)
std::wstring g_button3Url; // URL for button 3 (提示)
std::wstring g_button4Url; // URL for button 4 (注意)
std::wstring g_button5Url; // URL for button 5 (思考)
DWORD g_rollDuration = 2000;        // 可配置的滚动持续时间（默认800ms）

static std::wstring getConfigPathGlobal() {
    wchar_t buf[MAX_PATH];
    DWORD len = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
    std::wstring path;
    if (len > 0 && len < MAX_PATH) path = std::wstring(buf) + L"\\TeachingWindow";
    else path = L".";
    return path + L"\\config.json";
}

static void loadConfig() {
    g_configStudents.clear();
    g_innerUrl.clear();
    g_manualIp.clear();
    std::wstring cfg = getConfigPathGlobal();
    FILE* f = _wfopen(cfg.c_str(), L"r, ccs=UTF-8");
    if (!f) return;
    std::wstring content;
    wchar_t chunk[1024];
    while (fgetws(chunk, 1024, f)) content += chunk;
    fclose(f);
    // manual_ip
    size_t pos = content.find(L"\"manual_ip\"");
    if (pos != std::wstring::npos) {
        size_t colon = content.find(L':', pos);
        size_t q1 = content.find(L'"', colon);
        size_t q2 = content.find(L'"', q1+1);
        if (q1!=std::wstring::npos && q2!=std::wstring::npos) g_manualIp = content.substr(q1+1, q2-q1-1);
    }
    // inner_url
    pos = content.find(L"\"inner_url\"");
    if (pos != std::wstring::npos) {
        size_t colon = content.find(L':', pos);
        size_t q1 = content.find(L'"', colon);
        size_t q2 = content.find(L'"', q1+1);
        if (q1!=std::wstring::npos && q2!=std::wstring::npos) g_innerUrl = content.substr(q1+1, q2-q1-1);
    }
    // students (stored as single string with '|' delimiter)
    pos = content.find(L"\"students\"");
    if (pos != std::wstring::npos) {
        size_t colon = content.find(L':', pos);
        size_t q1 = content.find(L'"', colon);
        size_t q2 = content.find(L'"', q1+1);
        if (q1!=std::wstring::npos && q2!=std::wstring::npos) {
            std::wstring s = content.substr(q1+1, q2-q1-1);
            // split by '|'
            std::wstring cur; for (wchar_t c: s) { if (c==L'|') { if (!cur.empty()) g_configStudents.push_back(cur); cur.clear(); } else cur.push_back(c); }
            if (!cur.empty()) g_configStudents.push_back(cur);
        }
    }
    // button3_url
    pos = content.find(L"\"button3_url\"");
    if (pos != std::wstring::npos) {
        size_t colon = content.find(L':', pos);
        size_t q1 = content.find(L'"', colon);
        size_t q2 = content.find(L'"', q1+1);
        if (q1!=std::wstring::npos && q2!=std::wstring::npos) g_button3Url = content.substr(q1+1, q2-q1-1);
    }
    // button4_url
    pos = content.find(L"\"button4_url\"");
    if (pos != std::wstring::npos) {
        size_t colon = content.find(L':', pos);
        size_t q1 = content.find(L'"', colon);
        size_t q2 = content.find(L'"', q1+1);
        if (q1!=std::wstring::npos && q2!=std::wstring::npos) g_button4Url = content.substr(q1+1, q2-q1-1);
    }
    // button5_url
    pos = content.find(L"\"button5_url\"");
    if (pos != std::wstring::npos) {
        size_t colon = content.find(L':', pos);
        size_t q1 = content.find(L'"', colon);
        size_t q2 = content.find(L'"', q1+1);
        if (q1!=std::wstring::npos && q2!=std::wstring::npos) g_button5Url = content.substr(q1+1, q2-q1-1);
    }
    // roll_duration
    pos = content.find(L"\"roll_duration\"");
    if (pos != std::wstring::npos) {
        size_t colon = content.find(L':', pos);
        size_t q1 = content.find(L'"', colon);
        size_t q2 = content.find(L'"', q1+1);
        if (q1!=std::wstring::npos && q2!=std::wstring::npos) {
            std::wstring durStr = content.substr(q1+1, q2-q1-1);
            g_rollDuration = _wtoi(durStr.c_str());
            // 确保是有效值
            if (g_rollDuration != 500 && g_rollDuration != 800 && g_rollDuration != 1000 && g_rollDuration != 1500) {
                g_rollDuration = 800; // 默认值
            }
        }
    } else {
        g_rollDuration = 800; // 默认值
    }
}

static bool saveConfig() {
    std::wstring cfg = getConfigPathGlobal();
    size_t p = cfg.find_last_of(L"\\/");
    std::wstring dir = (p==std::wstring::npos)? L"." : cfg.substr(0,p);
    CreateDirectoryW(dir.c_str(), NULL);
    FILE* f = _wfopen(cfg.c_str(), L"w, ccs=UTF-8");
    if (!f) return false;
    // join students with |
    std::wstring stu;
    for (size_t i=0;i<g_configStudents.size();++i) {
        if (i) stu += L"|";
        stu += g_configStudents[i];
    }
    fwprintf(f, L"{\n  \"manual_ip\": \"%ls\",\n  \"inner_url\": \"%ls\",\n  \"students\": \"%ls\",\n  \"button3_url\": \"%ls\",\n  \"button4_url\": \"%ls\",\n  \"button5_url\": \"%ls\",\n  \"roll_duration\": \"%d\"\n}\n",
             g_manualIp.c_str(), g_innerUrl.c_str(), stu.c_str(),
             g_button3Url.c_str(), g_button4Url.c_str(), g_button5Url.c_str(), g_rollDuration);
    fclose(f);
    return true;
}

// 美化弹出窗口相关变量
HWND g_rollCallWindow = NULL;
const wchar_t* g_selectedStudent = NULL;

// 点名窗口拖动相关变量
bool g_rollCallDragging = false;
POINT g_rollCallDragOffset;

//// 随机滚动动画相关变量
bool g_isRolling = false;           // 是否正在滚动
DWORD g_rollStartTime = 0;          // 滚动开始时间
const DWORD ROLL_DURATION = 2000;   // 滚动持续时间（2秒，默认值）
const UINT_PTR TIMER_ID = 1;        // 定时器ID

// 105班学生名单
const wchar_t* students105[] = {
    L"李唐玄烨", L"张鑫磊", L"陆培林", L"包卓林", L"刘锦承",
    L"韩泽恺", L"周谦宇", L"冯畅", L"邓润松",
    L"黄轲", L"李添", L"李希振", L"方世博", L"胡泽昱",
    L"罗灵烨", L"董嘉豪", L"张腾文", L"程宇轩", L"毛智楷",
    L"刘鹏玮", L"张羽杰", L"程镇江", L"黄俊轩", L"向奕豪",
    L"冯海文", L"皮澎淼", L"曹振杨", L"陈姝涵", L"陈梦琪",
    L"柯颖", L"黄婉琪", L"杨泽茜", L"黄亦诺", L"潘采萱",
    L"姜槿瑜", L"陈明玥", L"彭子慕", L"南佳怡", L"李耘锦",
    L"华诗语", L"吕雨竹", L"孟子涵", L"胡芸", L"左希源",
    L"张乐彤", L"罗泽晴", L"伍柃颖", L"雷诺"
};
const int STUDENT_COUNT = 49; // 总共49名学生

// 随机点名函数
const wchar_t* getRandomStudent() {
    static bool initialized = false;
    if (!initialized) {
        srand((unsigned int)time(NULL));
        initialized = true;
    }
    
    int randomIndex = rand() % STUDENT_COUNT;
    return students105[randomIndex];
}

// 美化点名弹出窗口的窗口过程
LRESULT CALLBACK RollCallWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // 获取窗口矩形
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            // 创建渐变背景 - 淡蓝色到白色
            HBRUSH bgBrush = CreateSolidBrush(RGB(240, 248, 255));
            FillRect(hdc, &rect, bgBrush);
            
            // 绘制装饰性边框 - 双重边框效果
            HPEN outerBorderPen = CreatePen(PS_SOLID, 4, RGB(70, 130, 220));
            HPEN innerBorderPen = CreatePen(PS_SOLID, 2, RGB(135, 206, 250));
            HPEN oldPen = (HPEN)SelectObject(hdc, outerBorderPen);
            
            // 外边框
            Rectangle(hdc, 2, 2, rect.right - 2, rect.bottom - 2);
            
            // 内边框
            SelectObject(hdc, innerBorderPen);
            Rectangle(hdc, 6, 6, rect.right - 6, rect.bottom - 6);
            
            // 绘制标题区域背景 - 渐变效果
            RECT titleRect = {15, 20, rect.right - 15, 70};
            HBRUSH titleBrush = CreateSolidBrush(RGB(70, 130, 220));
            FillRect(hdc, &titleRect, titleBrush);
            
            // 添加标题区域的圆角效果（通过绘制圆角矩形模拟）
            HPEN titleBorderPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
            SelectObject(hdc, titleBorderPen);
            RoundRect(hdc, titleRect.left, titleRect.top, titleRect.right, titleRect.bottom, 10, 10);
            
            // 绘制标题文字
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            
            HFONT titleFont = CreateFontW(
                28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"微软雅黑"
            );
            HFONT oldFont = (HFONT)SelectObject(hdc, titleFont);
            
            DrawTextW(hdc, L"🎯 105班随机点名", -1, &titleRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            // 绘制装饰性文字
            SetTextColor(hdc, RGB(80, 80, 80));
            HFONT subtitleFont = CreateFontW(
                18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"微软雅黑"
            );
            SelectObject(hdc, subtitleFont);
            
            RECT subtitleRect = {20, 90, rect.right - 20, 120};
            DrawTextW(hdc, L"✨ 被点到的同学是 ✨", -1, &subtitleRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            // 绘制学生姓名 - 88号字体，华丽效果
            if (g_selectedStudent) {
                // 创建88号华文行楷字体
                HFONT nameFont = CreateFontW(
                    108, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"华文行楷"
                );
                SelectObject(hdc, nameFont);
                
                RECT nameRect = {20, 130, rect.right - 20, 250};
                
                // 确定要显示的姓名（动画中显示随机姓名，结束后显示最终结果）
                const wchar_t* displayName = g_selectedStudent;
                if (g_isRolling) {
                    // 动画进行中，显示随机滚动的姓名
                    displayName = getRandomStudent();
                }
                
                // 添加多层阴影效果，营造立体感
                // 第一层阴影 - 深灰色
                SetTextColor(hdc, RGB(150, 150, 150));
                RECT shadow1Rect = nameRect;
                OffsetRect(&shadow1Rect, 4, 4);
                DrawTextW(hdc, displayName, -1, &shadow1Rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                
                // 第二层阴影 - 浅灰色
                SetTextColor(hdc, RGB(200, 200, 200));
                RECT shadow2Rect = nameRect;
                OffsetRect(&shadow2Rect, 2, 2);
                DrawTextW(hdc, displayName, -1, &shadow2Rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                
                // 主文字 - 红色渐变效果（动画中使用不同颜色增加动感）
                if (g_isRolling) {
                    // 动画中使用橙色，增加动感
                    SetTextColor(hdc, RGB(255, 140, 0));
                } else {
                    // 最终结果使用红色
                    SetTextColor(hdc, RGB(220, 20, 60));
                }
                DrawTextW(hdc, displayName, -1, &nameRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                
                DeleteObject(nameFont);
            }
            
            // 绘制底部装饰文字
            SetTextColor(hdc, RGB(100, 149, 237));
            HFONT hintFont = CreateFontW(
                16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"微软雅黑"
            );
            SelectObject(hdc, hintFont);
            
            RECT hintRect = {20, 240, rect.right - 20, 270};
            DrawTextW(hdc, L"🌟 请这位同学回答问题！🌟", -1, &hintRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            // 绘制两个美化按钮：再次点名 和 关闭
            // 使用固定位置确保按钮显示
            
            // 再次点名按钮（左侧，绿色）- 固定位置
            RECT rollAgainButtonRect = {30, 320, 180, 360};
            HBRUSH rollAgainBrush = CreateSolidBrush(RGB(80, 200, 80));
            FillRect(hdc, &rollAgainButtonRect, rollAgainBrush);
            
            // 关闭按钮（右侧，红色）- 固定位置
            RECT closeButtonRect = {300, 320, 450, 360};
            HBRUSH closeBrush = CreateSolidBrush(RGB(220, 80, 80));
            FillRect(hdc, &closeButtonRect, closeBrush);
            
            // 按钮边框和圆角效果
            HPEN buttonBorderPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
            SelectObject(hdc, buttonBorderPen);
            
            // 绘制再次点名按钮边框
            RoundRect(hdc, rollAgainButtonRect.left, rollAgainButtonRect.top, 
                     rollAgainButtonRect.right, rollAgainButtonRect.bottom, 10, 10);
            
            // 绘制关闭按钮边框
            RoundRect(hdc, closeButtonRect.left, closeButtonRect.top, 
                     closeButtonRect.right, closeButtonRect.bottom, 10, 10);
            
            // 按钮文字字体
            HFONT buttonFont = CreateFontW(
                16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"微软雅黑"
            );
            SelectObject(hdc, buttonFont);
            
            // 绘制"再来一个"按钮文字 - 26号蓝色字体
            HFONT rollAgainFont = CreateFontW(
                26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"微软雅黑"
            );
            SelectObject(hdc, rollAgainFont);
            
            SetTextColor(hdc, RGB(0, 100, 200)); // 蓝色文字
            DrawTextW(hdc, L"🎲 下一位", -1, &rollAgainButtonRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            // 绘制关闭按钮文字 - 恢复原字体
            SelectObject(hdc, buttonFont);
            SetTextColor(hdc, RGB(0, 100, 255)); // 白色文字
            DrawTextW(hdc, L"❌ 关闭", -1, &closeButtonRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            // 清理"再来一个"按钮字体
            DeleteObject(rollAgainFont);
            
            // 清理按钮资源
            DeleteObject(rollAgainBrush);
            
            // 清理资源
            SelectObject(hdc, oldFont);
            SelectObject(hdc, oldPen);
            DeleteObject(titleFont);
            DeleteObject(subtitleFont);
            DeleteObject(hintFont);
            DeleteObject(buttonFont);
            DeleteObject(bgBrush);
            DeleteObject(titleBrush);
            DeleteObject(closeBrush);
            DeleteObject(outerBorderPen);
            DeleteObject(innerBorderPen);
            DeleteObject(titleBorderPen);
            DeleteObject(buttonBorderPen);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            // 再次点名按钮区域 - 固定坐标
            RECT rollAgainButtonRect = {30, 320, 180, 360};
            // 关闭按钮区域 - 固定坐标
            RECT closeButtonRect = {300, 320, 450, 360};
            
            if (x >= rollAgainButtonRect.left && x <= rollAgainButtonRect.right &&
                y >= rollAgainButtonRect.top && y <= rollAgainButtonRect.bottom) {
                // 点击了再次点名按钮，开始2秒随机滚动动画
                if (!g_isRolling) {
                    g_isRolling = true;
                    g_rollStartTime = GetTickCount();
                    g_selectedStudent = getRandomStudent(); // 预先选定最终结果
                    
                    // 启动定时器，每100毫秒更新一次动画
                    SetTimer(hwnd, TIMER_ID, 100, NULL);
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            else if (x >= closeButtonRect.left && x <= closeButtonRect.right &&
                     y >= closeButtonRect.top && y <= closeButtonRect.bottom) {
                // 点击了关闭按钮
                DestroyWindow(hwnd);
                g_rollCallWindow = NULL;
            }
            else {
                // 点击在其他区域，开始拖动窗口
                g_rollCallDragging = true;
                SetCapture(hwnd);
                
                POINT cursorPos;
                GetCursorPos(&cursorPos);
                RECT windowRect;
                GetWindowRect(hwnd, &windowRect);
                
                g_rollCallDragOffset.x = cursorPos.x - windowRect.left;
                g_rollCallDragOffset.y = cursorPos.y - windowRect.top;
            }
            return 0;
        }
        
        case WM_MOUSEMOVE: {
            if (g_rollCallDragging) {
                POINT cursorPos;
                GetCursorPos(&cursorPos);
                
                int newX = cursorPos.x - g_rollCallDragOffset.x;
                int newY = cursorPos.y - g_rollCallDragOffset.y;
                
                SetWindowPos(hwnd, NULL, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            return 0;
        }
        
        case WM_LBUTTONUP: {
            if (g_rollCallDragging) {
                g_rollCallDragging = false;
                ReleaseCapture();
            }
            return 0;
        }
        
        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE || wParam == VK_RETURN || wParam == VK_SPACE) {
                DestroyWindow(hwnd);
                g_rollCallWindow = NULL;
            }
            return 0;
        }
        
        case WM_TIMER: {
            if (wParam == TIMER_ID && g_isRolling) {
                DWORD currentTime = GetTickCount();
                DWORD elapsed = currentTime - g_rollStartTime;

                if (elapsed < g_rollDuration) { // 使用可配置的动画时间
                    // 动画进行中，重新绘制显示随机姓名
                    InvalidateRect(hwnd, NULL, TRUE); // 重新绘制
                } else {
                    // 动画结束，停止定时器，显示最终结果
                    KillTimer(hwnd, TIMER_ID);
                    g_isRolling = false;

                    // 播放系统beep声提示点名完成
                    MessageBeep(MB_ICONASTERISK); // 使用系统默认提示音

                    InvalidateRect(hwnd, NULL, TRUE); // 最后一次重绘显示最终结果
                }
            }
            return 0;
        }
        
        case WM_DESTROY: {
            // 清理定时器
            KillTimer(hwnd, TIMER_ID);
            g_rollCallWindow = NULL;
            return 0;
        }
    }
    
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// 显示美化的点名弹出窗口
void ShowBeautifulRollCallWindow(const wchar_t* studentName) {
    if (g_rollCallWindow) {
        // 如果窗口已存在，先关闭
        DestroyWindow(g_rollCallWindow);
    }
    
    g_selectedStudent = studentName;
    
    // 初始化动画状态
    g_isRolling = true;
    g_rollStartTime = GetTickCount();
    
    // 注册窗口类
    const wchar_t ROLL_CLASS_NAME[] = L"BeautifulRollCallWindow";
    
    WNDCLASSW wc = {};
    wc.lpfnWndProc = RollCallWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = ROLL_CLASS_NAME;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(240, 248, 255));
    
    RegisterClassW(&wc);
    
    // 计算窗口位置（屏幕中央）
    int windowWidth = 480;  // 增加宽度以容纳两个按钮（保持原始大小）
    int windowHeight = 380; // 进一步增加高度以完整显示按钮（保持原始大小）
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;
    
    // 创建窗口
    g_rollCallWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        ROLL_CLASS_NAME,
        L"105班随机点名",
        WS_POPUP | WS_VISIBLE,
        x, y, windowWidth, windowHeight,
        g_hwnd, NULL, GetModuleHandle(NULL), NULL
    );
    
    if (g_rollCallWindow) {
        ShowWindow(g_rollCallWindow, SW_SHOW);
        UpdateWindow(g_rollCallWindow);
        SetFocus(g_rollCallWindow);
        
        // 启动2秒随机滚动动画
        SetTimer(g_rollCallWindow, TIMER_ID, 100, NULL); // 每100毫秒更新一次
    }
}

// 设置窗口的全局句柄
HWND g_hSettingsWindow = NULL;

// 设置窗口的窗口过程函数
LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // 调整窗口大小以适应更多控件
            SetWindowPos(hwnd, NULL, 0, 0, S(450), S(450), SWP_NOMOVE | SWP_NOZORDER); // 增加高度

            // 字体设置
            HFONT hFont = CreateFontW(S(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"微软雅黑");

            // URL 设置组
            int urlY = S(20);
            // 内页 URL 标签
            HWND staticUrl = CreateWindowW(L"STATIC", L"电脑 URL：", WS_CHILD | WS_VISIBLE, S(20), urlY, S(80), S(20), hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessage(staticUrl, WM_SETFONT, (WPARAM)hFont, TRUE);
            // 内页 URL 编辑框
            HWND editUrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_innerUrl.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, S(110), urlY, S(300), S(24), hwnd, (HMENU)3001, GetModuleHandle(NULL), NULL);
            SendMessage(editUrl, WM_SETFONT, (WPARAM)hFont, TRUE);

            urlY += S(35);
            // TV URL 标签
            HWND staticBtn3 = CreateWindowW(L"STATIC", L"TV URL：", WS_CHILD | WS_VISIBLE, S(20), urlY, S(80), S(20), hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessage(staticBtn3, WM_SETFONT, (WPARAM)hFont, TRUE);
            // TV URL 编辑框
            HWND editBtn3 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_button3Url.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, S(110), urlY, S(300), S(24), hwnd, (HMENU)3005, GetModuleHandle(NULL), NULL);
            SendMessage(editBtn3, WM_SETFONT, (WPARAM)hFont, TRUE);

            urlY += S(35);
            // 备课 URL 标签
            HWND staticBtn4 = CreateWindowW(L"STATIC", L"备课 URL：", WS_CHILD | WS_VISIBLE, S(20), urlY, S(80), S(20), hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessage(staticBtn4, WM_SETFONT, (WPARAM)hFont, TRUE);
            // 备课 URL 编辑框
            HWND editBtn4 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_button4Url.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, S(110), urlY, S(300), S(24), hwnd, (HMENU)3006, GetModuleHandle(NULL), NULL);
            SendMessage(editBtn4, WM_SETFONT, (WPARAM)hFont, TRUE);

            urlY += S(35);
            // 班级 URL 标签
            HWND staticBtn5 = CreateWindowW(L"STATIC", L"班级 URL：", WS_CHILD | WS_VISIBLE, S(20), urlY, S(80), S(20), hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessage(staticBtn5, WM_SETFONT, (WPARAM)hFont, TRUE);
            // 班级 URL 编辑框
            HWND editBtn5 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_button5Url.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, S(110), urlY, S(300), S(24), hwnd, (HMENU)3007, GetModuleHandle(NULL), NULL);
            SendMessage(editBtn5, WM_SETFONT, (WPARAM)hFont, TRUE);

            // 学生名单组
            int stuY = urlY + S(45); // 在URL组下方留出间距
            // 学生名单标签
            HWND staticStu = CreateWindowW(L"STATIC", L"学生名单（空格或逗号分隔）：", WS_CHILD | WS_VISIBLE, S(20), stuY, S(250), S(20), hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessage(staticStu, WM_SETFONT, (WPARAM)hFont, TRUE);
            // 点名动画间隔设置
            int animY = stuY + S(45); // 在学生名单下方留出间距
            // 动画间隔标签
            HWND staticAnim = CreateWindowW(L"STATIC", L"点名动画间隔：", WS_CHILD | WS_VISIBLE, S(20), animY, S(120), S(20), hwnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessage(staticAnim, WM_SETFONT, (WPARAM)hFont, TRUE);

            // 动画间隔组合框 (选择 500ms, 800ms, 1s, 1500ms)
            // 创建组合框控件
            HWND comboAnim = CreateWindowW(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL,
                S(140), animY - S(2), S(120), S(100), hwnd, (HMENU)3008, GetModuleHandle(NULL), NULL);
            SendMessage(comboAnim, WM_SETFONT, (WPARAM)hFont, TRUE);

            // 添加选项到组合框
            SendMessageW(comboAnim, CB_ADDSTRING, 0, (LPARAM)L"500ms");
            SendMessageW(comboAnim, CB_ADDSTRING, 0, (LPARAM)L"800ms");
            SendMessageW(comboAnim, CB_ADDSTRING, 0, (LPARAM)L"1s");
            SendMessageW(comboAnim, CB_ADDSTRING, 0, (LPARAM)L"1.5s");

            // 根据当前值设置选中项
            int selectedIndex = 1; // 默认为800ms
            if (g_rollDuration == 500) selectedIndex = 0;
            else if (g_rollDuration == 1000) selectedIndex = 2;
            else if (g_rollDuration == 1500) selectedIndex = 3;
            SendMessageW(comboAnim, CB_SETCURSEL, selectedIndex, 0);

            // 更新按钮位置
            int btnY = animY + S(50); // 在动画间隔设置下方留出间距
            // 保存按钮
            HWND btnSave = CreateWindowW(L"BUTTON", L"保存", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, S(80), btnY, S(100), S(30), hwnd, (HMENU)3003, GetModuleHandle(NULL), NULL);
            SendMessage(btnSave, WM_SETFONT, (WPARAM)hFont, TRUE);
            // 取消按钮
            HWND btnCancel = CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, S(250), btnY, S(100), S(30), hwnd, (HMENU)3004, GetModuleHandle(NULL), NULL);
            SendMessage(btnCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

            // 存储字体句柄，以便在WM_DESTROY中销毁
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)hFont);
            return 0;
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case 3003: // 保存按钮ID
                    {
                        wchar_t buf[2048];
                        GetWindowTextW(GetDlgItem(hwnd, 3001), buf, 2048); // 获取内页 URL
                        g_innerUrl = buf;

                        wchar_t buf2[4096];
                        GetWindowTextW(GetDlgItem(hwnd, 3002), buf2, 4096); // 获取学生名单
                        // split by space or comma
                        g_configStudents.clear();
                        std::wistringstream iss(buf2);
                        std::wstring token;
                        while (std::getline(iss, token, L' ')) {
                            if (!token.empty()) {
                                std::wistringstream iss2(token);
                                std::wstring subToken;
                                while (std::getline(iss2, subToken, L',')) {
                                    if (!subToken.empty()) g_configStudents.push_back(subToken);
                                }
                            }
                        }
                        // Fallback for '|' if no space/comma found or if user still uses it
                        if (g_configStudents.empty() && std::wstring(buf2).find(L'|') != std::wstring::npos) {
                            g_configStudents.clear();
                            std::wistringstream iss3(buf2);
                            std::wstring subToken;
                            while (std::getline(iss3, subToken, L'|')) {
                                if (!subToken.empty()) g_configStudents.push_back(subToken);
                            }
                        }

                        // 获取按钮3 URL
                        GetWindowTextW(GetDlgItem(hwnd, 3005), buf, 2048);
                        g_button3Url = buf;
                        // 获取按钮4 URL
                        GetWindowTextW(GetDlgItem(hwnd, 3006), buf, 2048);
                        g_button4Url = buf;
                        // 获取按钮5 URL
                        GetWindowTextW(GetDlgItem(hwnd, 3007), buf, 2048);
                        g_button5Url = buf;

                        // 获取动画间隔设置
                        HWND combo = GetDlgItem(hwnd, 3008);
                        int selectedIndex = SendMessageW(combo, CB_GETCURSEL, 0, 0);
                        if (selectedIndex != CB_ERR) {
                            switch (selectedIndex) {
                                case 0: g_rollDuration = 500; break;
                                case 1: g_rollDuration = 800; break;
                                case 2: g_rollDuration = 1000; break;
                                case 3: g_rollDuration = 1500; break;
                                default: g_rollDuration = 800; break;
                            }
                        }

                        if (saveConfig()) {
                            MessageBoxW(hwnd, L"配置已保存成功！", L"保存成功", MB_OK | MB_ICONINFORMATION);
                        } else {
                            MessageBoxW(hwnd, L"配置保存失败！", L"保存失败", MB_OK | MB_ICONERROR);
                        }
                        DestroyWindow(hwnd); // 关闭窗口
                    }
                    break;
                case 3004: // 取消按钮ID
                    DestroyWindow(hwnd); // 关闭窗口
                    break;
            }
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd); // 处理关闭按钮或Alt+F4
            return 0;
        case WM_DESTROY:
            g_hSettingsWindow = NULL; // 清除全局句柄
            return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// 窗口过程函数
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // 获取客户区矩形
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            
            // 1. 绘制标题拖动区域（顶部，深蓝色）
            RECT titleRect = {0, 0, WINDOW_WIDTH, TITLE_HEIGHT};
            HBRUSH titleBrush = CreateSolidBrush(RGB(30, 60, 120));
            FillRect(hdc, &titleRect, titleBrush);
            
            // 在标题区域绘制文字
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            
            // 创建标题字体
            HFONT titleFont = CreateFontW(
                S(10), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"微软雅黑"
            );
            HFONT oldFont = (HFONT)SelectObject(hdc, titleFont);
            
            DrawTextW(hdc, L"📚 教学工具 - 拖动", -1, &titleRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            // 2. 绘制教学功能按钮区域（中间，6个紧凑按钮）
            int buttonWidth = WINDOW_WIDTH / 3;  // 每个按钮宽度 = 40像素
            int buttonHeight = BUTTON_AREA_HEIGHT / 2;  // 每个按钮高度 = 27像素
            
            // 教学按钮颜色和功能
            COLORREF buttonColors[6] = {
                RGB(220, 80, 80),   // 红色 - 网页按钮
                RGB(80, 200, 80),   // 绿色 - 点名系统
                RGB(80, 120, 220),  // 蓝色 - 提示信息
                RGB(220, 180, 60),  // 橙色 - 注意事项
                RGB(180, 80, 200),  // 紫色 - 思考题
                RGB(60, 180, 200)   // 青色 - 补充说明
            };
            
            // 教学按钮文字（紧凑版）
            const wchar_t* buttonTexts[6] = {
                L"电脑", L"点名", L"TV", 
                L"备课", L"班级", L"补充"
            };
            
            // 创建按钮字体（加大文字）
            HFONT buttonFont = CreateFontW(
                S(11), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"微软雅黑"
            );
            SelectObject(hdc, buttonFont);
            
            // 绘制6个教学功能按钮（3x2布局）
            for (int i = 0; i < 6; i++) {
                int row = i / 3;  // 行号 (0或1)
                int col = i % 3;  // 列号 (0,1,2)
                
                RECT buttonRect = {
                    col * buttonWidth + S(1),                              // left (加1像素边距)
                    TITLE_HEIGHT + row * buttonHeight + S(1),             // top (从标题区域下方开始)
                    (col + 1) * buttonWidth - S(1),                       // right (减1像素边距)
                    TITLE_HEIGHT + (row + 1) * buttonHeight - S(1)        // bottom (减1像素边距)
                };
                
                // 绘制按钮背景
                HBRUSH buttonBrush = CreateSolidBrush(buttonColors[i]);
                FillRect(hdc, &buttonRect, buttonBrush);
                
                // 绘制按钮边框
                HPEN borderPen = CreatePen(PS_SOLID, S(1), RGB(255, 255, 255));
                HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
                
                MoveToEx(hdc, buttonRect.left, buttonRect.top, NULL);
                LineTo(hdc, buttonRect.right, buttonRect.top);
                LineTo(hdc, buttonRect.right, buttonRect.bottom);
                LineTo(hdc, buttonRect.left, buttonRect.bottom);
                LineTo(hdc, buttonRect.left, buttonRect.top);
                
                SelectObject(hdc, oldPen);
                DeleteObject(borderPen);
                
                // 绘制按钮文字
                SetTextColor(hdc, RGB(255, 255, 255));
                DrawTextW(hdc, buttonTexts[i], -1, &buttonRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                
                DeleteObject(buttonBrush);
            }
            
            // 3. 绘制关闭区域（底部，红色）
            RECT closeRect = {0, WINDOW_HEIGHT - CLOSE_HEIGHT, WINDOW_WIDTH, WINDOW_HEIGHT};
            HBRUSH closeBrush = CreateSolidBrush(RGB(180, 50, 50));
            FillRect(hdc, &closeRect, closeBrush);
            
            // 在关闭区域绘制文字
            SetTextColor(hdc, RGB(255, 255, 255));
            DrawTextW(hdc, L"❌ 关闭", -1, &closeRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            // 清理资源
            SelectObject(hdc, oldFont);
            DeleteObject(titleFont);
            DeleteObject(buttonFont);
            DeleteObject(titleBrush);
            DeleteObject(closeBrush);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            
            // 检查点击位置
            if (y < TITLE_HEIGHT) {
                // 点击在标题拖动区域
                g_isDragging = true;
                SetCapture(hwnd);
                
                POINT cursorPos;
                GetCursorPos(&cursorPos);
                RECT windowRect;
                GetWindowRect(hwnd, &windowRect);
                
                g_dragOffset.x = cursorPos.x - windowRect.left;
                g_dragOffset.y = cursorPos.y - windowRect.top;
            }
            else if (y >= WINDOW_HEIGHT - CLOSE_HEIGHT) {
                // 点击在关闭区域
                PostQuitMessage(0);
            }
            else if (y >= TITLE_HEIGHT && y < WINDOW_HEIGHT - CLOSE_HEIGHT) {
                // 点击在按钮区域
                int buttonY = y - TITLE_HEIGHT;  // 相对于按钮区域的Y坐标
                int buttonRow = buttonY / (BUTTON_AREA_HEIGHT / 2);
                int buttonCol = x / (WINDOW_WIDTH / 3);
                int buttonIndex = buttonRow * 3 + buttonCol;
                
                if (buttonIndex >= 0 && buttonIndex < 6) {
                    // 按钮功能响应
                    if (buttonIndex == 0) {
                        // 按钮1：尝试打开网页，失败时弹出输入框更改后两段并保存到 json

                        auto getConfigPath = []() -> std::wstring {
                            wchar_t buf[MAX_PATH];
                            DWORD len = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
                            std::wstring path;
                            if (len > 0 && len < MAX_PATH) {
                                path = std::wstring(buf) + L"\\TeachingWindow";
                            } else {
                                path = L".";
                            }
                            return path + L"\\config.json";
                        };

                        auto readManualIp = [&](std::wstring &outIp)->bool {
                            std::wstring cfg = getConfigPath();
                            FILE* f = _wfopen(cfg.c_str(), L"r, ccs=UTF-8");
                            if (!f) return false;
                            std::wstring content;
                            wchar_t chunk[1024];
                            while (fgetws(chunk, 1024, f)) content += chunk;
                            fclose(f);
                            size_t pos = content.find(L"\"manual_ip\"");
                            if (pos == std::wstring::npos) return false;
                            size_t colon = content.find(L':', pos);
                            if (colon == std::wstring::npos) return false;
                            size_t firstQuote = content.find(L'\"', colon);
                            if (firstQuote == std::wstring::npos) return false;
                            size_t secondQuote = content.find(L'\"', firstQuote + 1);
                            if (secondQuote == std::wstring::npos) return false;
                            outIp = content.substr(firstQuote + 1, secondQuote - firstQuote - 1);
                            return true;
                        };

                        auto saveManualIp = [&](const std::wstring &ip)->bool {
                            std::wstring cfg = getConfigPath();
                            size_t p = cfg.find_last_of(L"\\/");
                            std::wstring dir = (p==std::wstring::npos)? L"." : cfg.substr(0,p);
                            CreateDirectoryW(dir.c_str(), NULL);
                            FILE* f = _wfopen(cfg.c_str(), L"w, ccs=UTF-8");
                            if (!f) return false;
                            fwprintf(f, L"{\n  \"manual_ip\": \"%ls\"\n}\n", ip.c_str());
                            fclose(f);
                            return true;
                        };

                        auto tryOpen = [&](const std::wstring &ip)->bool {
                            std::wstring url = L"http://" + ip;
                            HINSTANCE h = ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
                            return (INT_PTR)h > 32;
                        };

                        // 如果设置了 inner url 优先使用
                        if (!g_innerUrl.empty()) {
                            ShellExecuteW(NULL, L"open", g_innerUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
                        } else {
                            std::wstring ip;
                            if (!readManualIp(ip)) ip = L"192.168.6.155";
                            if (!tryOpen(ip)) {
                            // 失败，弹出小对话修改后两段
                            std::vector<int> parts(4,0);
                            {
                                std::wistringstream iss(ip);
                                std::wstring token; int idx=0;
                                while (std::getline(iss, token, L'.') && idx<4) parts[idx++] = _wtoi(token.c_str());
                            }

                            // 创建简单对话
                            HWND hDlg = CreateWindowExW(0, L"STATIC", L"修改IP", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                CW_USEDEFAULT, CW_USEDEFAULT, S(300), S(160), hwnd, NULL, GetModuleHandle(NULL), NULL);
                            if (hDlg) {
                                // edit controls
                                CreateWindowW(L"STATIC", L"第三段：", WS_CHILD | WS_VISIBLE, S(20), S(20), S(80), S(20), hDlg, NULL, NULL, NULL);
                                HWND e3 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", NULL, WS_CHILD | WS_VISIBLE | ES_NUMBER, S(100), S(20), S(60), S(22), hDlg, NULL, NULL, NULL);
                                wchar_t buf3[8]; swprintf(buf3, 8, L"%d", parts[2]); SetWindowTextW(e3, buf3);

                                CreateWindowW(L"STATIC", L"第四段：", WS_CHILD | WS_VISIBLE, S(20), S(60), S(80), S(20), hDlg, NULL, NULL, NULL);
                                HWND e4 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", NULL, WS_CHILD | WS_VISIBLE | ES_NUMBER, S(100), S(60), S(60), S(22), hDlg, NULL, NULL, NULL);
                                wchar_t buf4[8]; swprintf(buf4, 8, L"%d", parts[3]); SetWindowTextW(e4, buf4);

                                HWND bOk = CreateWindowW(L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, S(60), S(100), S(80), S(28), hDlg, (HMENU)1001, NULL, NULL);
                                HWND bCancel = CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, S(160), S(100), S(80), S(28), hDlg, (HMENU)1002, NULL, NULL);

                                ShowWindow(hDlg, SW_SHOW);
                                UpdateWindow(hDlg);
                                // 简单消息循环直到窗口销毁
                                MSG msg2;
                                while (IsWindow(hDlg)) {
                                    if (PeekMessageW(&msg2, NULL, 0, 0, PM_REMOVE)) {
                                        if (msg2.message == WM_COMMAND) {
                                            int id = LOWORD(msg2.wParam);
                                            if (id == 1001 || id == 1002) {
                                                if (id == 1001) {
                                                    wchar_t tb[32]; GetWindowTextW(e3, tb, 32); int v3 = _wtoi(tb);
                                                    wchar_t tb2[32]; GetWindowTextW(e4, tb2, 32); int v4 = _wtoi(tb2);
                                                    // 组装IP
                                                    std::wistringstream iss(ip);
                                                    std::wstring t; std::vector<std::wstring> segs;
                                                    while (std::getline(iss, t, L'.')) segs.push_back(t);
                                                    if (segs.size() < 2) { segs = {L"192", L"168"}; }
                                                    std::wstringstream ss; ss<<segs[0]<<L"."<<segs[1]<<L"."<<v3<<L"."<<v4;
                                                    std::wstring newIp = ss.str();
                                                    saveManualIp(newIp);
                                                    tryOpen(newIp);
                                                }
                                                DestroyWindow(hDlg);
                                            }
                                        }
                                        TranslateMessage(&msg2);
                                        DispatchMessageW(&msg2);
                                    } else {
                                        Sleep(10);
                                    }
                                }
                            }
                            }
                        }
                    } 
                    else if (buttonIndex == 1) {
                        // 按钮2：105班随机点名系统 - 使用美化弹出窗口
                        const wchar_t* selectedStudent = getRandomStudent();
                        ShowBeautifulRollCallWindow(selectedStudent);
                    } 
                    else if (buttonIndex == 5) {
                        // 最后一个按钮：打开设置界面（编辑学生名单和内页 URL）
                        // 创建设置窗口
                        // 计算窗口位置（屏幕中央）
                        int windowWidth = S(420);
                        int windowHeight = S(300);
                        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
                        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
                        int x = (screenWidth - windowWidth) / 2;
                        int y = (screenHeight - windowHeight) / 2;

                        g_hSettingsWindow = CreateWindowExW(
                            WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, // 应用程序窗口样式
                            L"SettingsWindow", // 使用注册的设置窗口类名
                            L"设置",
                            WS_OVERLAPPEDWINDOW | WS_VISIBLE, // 标准重叠窗口样式，可拖动、最大化、最小化、关闭
                            x, y, windowWidth, windowHeight,
                            hwnd, NULL, GetModuleHandle(NULL), NULL
                        );

                        if (g_hSettingsWindow) {
                            ShowWindow(g_hSettingsWindow, SW_SHOW);
                            UpdateWindow(g_hSettingsWindow);
                            SetFocus(g_hSettingsWindow);
                        }
                    }
                    else {
                        // 其他按钮的教学功能
                        const wchar_t* messages[4] = {
                            L"💡 提示信息功能\n\n用于给学生适当提示，\n引导思考方向！",
                            L"⚠ 注意事项提醒\n\n用于提醒重要注意事项，\n避免常见错误！",
                            L"🤔 思考题发布\n\n用于发布思考题目，\n启发学生思维！",
                            L"📝 补充说明功能\n\n用于补充相关知识点，\n拓展学习内容！"
                        };
                        
                        const wchar_t* titles[4] = {
                            L"提示功能", L"注意提醒", L"思考启发", L"补充说明"
                        };
                        
                        // 根据按钮索引决定打开哪个URL或显示MessageBox
                        std::wstring urlToOpen;
                        if (buttonIndex == 2) { // 提示
                            urlToOpen = g_button3Url;
                        } else if (buttonIndex == 3) { // 注意
                            urlToOpen = g_button4Url;
                        } else if (buttonIndex == 4) { // 思考
                            urlToOpen = g_button5Url;
                        }

                        if (!urlToOpen.empty()) {
                            ShellExecuteW(NULL, L"open", urlToOpen.c_str(), NULL, NULL, SW_SHOWNORMAL);
                        } else {
                            // 如果URL为空，则显示原来的MessageBox
                            MessageBoxW(hwnd, messages[buttonIndex - 2], titles[buttonIndex - 2], 
                                      MB_OK | MB_TOPMOST | MB_ICONINFORMATION);
                        }
                    }
                }
            }
            return 0;
        }
        
        case WM_MOUSEMOVE: {
            if (g_isDragging) {
                POINT cursorPos;
                GetCursorPos(&cursorPos);
                
                int newX = cursorPos.x - g_dragOffset.x;
                int newY = cursorPos.y - g_dragOffset.y;
                
                SetWindowPos(hwnd, NULL, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            return 0;
        }
        
        case WM_LBUTTONUP: {
            if (g_isDragging) {
                g_isDragging = false;
                ReleaseCapture();
            }
            return 0;
        }
        
        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE) {
                PostQuitMessage(0);
            }
            return 0;
        }
        
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
    }
    
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 注册主窗口类
    const wchar_t MAIN_CLASS_NAME[] = L"TeachingFloatingWindowWithRoll";
    
    WNDCLASSW wcMain = {};
    wcMain.lpfnWndProc = WindowProc;
    wcMain.hInstance = hInstance;
    wcMain.lpszClassName = MAIN_CLASS_NAME;
    wcMain.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wcMain.hbrBackground = CreateSolidBrush(RGB(240, 240, 250));  // 浅色背景
    
    RegisterClassW(&wcMain);

    // 注册设置窗口类
    const wchar_t SETTINGS_CLASS_NAME[] = L"SettingsWindow";
    WNDCLASSW wcSettings = {};
    wcSettings.lpfnWndProc = SettingsWindowProc;
    wcSettings.hInstance = hInstance;
    wcSettings.lpszClassName = SETTINGS_CLASS_NAME;
    wcSettings.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wcSettings.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // 标准背景色
    RegisterClassW(&wcSettings);
    
    // 获取屏幕尺寸，计算右下角位置
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int startX = screenWidth - WINDOW_WIDTH - S(20);   // 距离右边缘20像素（已缩放）
    int startY = screenHeight - WINDOW_HEIGHT - S(60); // 距离底边缘60像素（已缩放，避开任务栏）
    
    // 创建主窗口
    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,  // 置顶 + 工具窗口
        MAIN_CLASS_NAME,
        L"教学工具箱",
        WS_POPUP,  // 无边框窗口
        startX, startY,  // 右下角位置
        WINDOW_WIDTH, WINDOW_HEIGHT,  // 窗口大小
        NULL, NULL, hInstance, NULL
    );
    
    if (g_hwnd == NULL) {
        return 0;
    }
    
    // 加载配置
    loadConfig();

    // 显示窗口
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    
    // 消息循环
    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    
    return 0;
}
