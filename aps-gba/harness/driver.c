#include <windows.h>
#include <stdio.h>
#include <string.h>

#define MAX_CONTROLS 64

/* A-Ptch is a GUI driven by window messages. 
   each step has to let the form settle before the next one reads it back.
   These waits are deliberately generous. */
#define LAUNCH_SETTLE_MS     4000  /* the form to finish drawing after launch     */
#define MODE_SETTLE_MS       1500  /* the controls to re-lay-out after a mode click */
#define FIELD_SETTLE_MS       200  /* one text field's edit to register            */
#define BEFORE_RUN_MS         500  /* everything to fall quiet before Run          */
#define RESULT_POLL_MS       1000  /* the gap between polls for the result dialog  */
#define RESULT_POLL_LIMIT      40  /* how many polls before giving the run up      */
#define POPUP_DISMISS_MS      300  /* the dialog to close after an IDOK            */

typedef struct {
    HWND hwnd;
    char className[128];
    char text[512];
    RECT rect;
} Control;

static Control controls[MAX_CONTROLS];
static int controlCount;
static DWORD targetProcessId;

static BOOL CALLBACK collectChild(HWND hwnd, LPARAM lp)
{
    if (controlCount < MAX_CONTROLS) {
        Control *control = &controls[controlCount];
        int scan, alreadySeen = 0;
        for (scan = 0; scan < controlCount; scan++)
            if (controls[scan].hwnd == hwnd) alreadySeen = 1;
        if (!alreadySeen) {
            control->hwnd = hwnd;
            GetClassNameA(hwnd, control->className, sizeof(control->className) - 1);
            control->text[0] = 0;
            SendMessageA(hwnd, WM_GETTEXT, sizeof(control->text) - 1, (LPARAM)control->text);
            GetWindowRect(hwnd, &control->rect);
            controlCount++;
        }
    }
    EnumChildWindows(hwnd, collectChild, 0);
    return TRUE;
}

static HWND formWindow;

static BOOL CALLBACK findForm(HWND hwnd, LPARAM lp)
{
    DWORD pid = 0;
    char className[128] = {0};
    GetWindowThreadProcessId(hwnd, &pid);
    GetClassNameA(hwnd, className, sizeof(className) - 1);
    if (pid == targetProcessId && strstr(className, "FormDC")) formWindow = hwnd;
    return TRUE;
}

static void refreshControls(void)
{
    controlCount = 0;
    EnumChildWindows(formWindow, collectChild, 0);
}

static HWND findByText(const char *classFragment, const char *textFragment)
{
    int i;
    for (i = 0; i < controlCount; i++)
        if (strstr(controls[i].className, classFragment) &&
            strstr(controls[i].text, textFragment))
            return controls[i].hwnd;
    return NULL;
}

static void dumpControls(const char *label)
{
    int i;
    printf("---- controls (%s) ----\n", label);
    for (i = 0; i < controlCount; i++)
        printf("  hwnd=%p %-26s top=%-5ld vis=%d text=\"%s\"\n",
               (void *)controls[i].hwnd, controls[i].className,
               controls[i].rect.top,
               IsWindowVisible(controls[i].hwnd) ? 1 : 0, controls[i].text);
    fflush(stdout);
}

/* Collect visible textboxes ordered top-to-bottom. */
static int collectTextBoxes(HWND *out, int maxOut)
{
    int scan, earlier, later, found = 0;
    for (scan = 0; scan < controlCount && found < maxOut; scan++)
        if (strstr(controls[scan].className, "TextBox") && IsWindowVisible(controls[scan].hwnd))
            out[found++] = controls[scan].hwnd;
    /* order them the way the eye reads the form: topmost first */
    for (earlier = 0; earlier < found; earlier++)
        for (later = earlier + 1; later < found; later++) {
            RECT earlierRect, laterRect;
            GetWindowRect(out[earlier], &earlierRect);
            GetWindowRect(out[later], &laterRect);
            if (laterRect.top < earlierRect.top) {
                HWND higher = out[earlier];
                out[earlier] = out[later];
                out[later] = higher;
            }
        }
    return found;
}

static BOOL CALLBACK reportPopup(HWND hwnd, LPARAM lp)
{
    DWORD pid = 0;
    char className[128] = {0};
    char text[512] = {0};
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != targetProcessId || hwnd == formWindow) return TRUE;
    GetClassNameA(hwnd, className, sizeof(className) - 1);
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (strcmp(className, "#32770") != 0) return TRUE;
    SendMessageA(hwnd, WM_GETTEXT, sizeof(text) - 1, (LPARAM)text);
    printf("  POPUP hwnd=%p class=%s title=\"%s\"\n", (void *)hwnd, className, text);
    controlCount = 0;
    EnumChildWindows(hwnd, collectChild, 0);
    dumpControls("popup children");
    printf("  -> dismissing with IDOK\n");
    fflush(stdout);
    SendMessageA(hwnd, WM_COMMAND, IDOK, 0);
    Sleep(POPUP_DISMISS_MS);
    if (IsWindow(hwnd) && IsWindowVisible(hwnd)) {
        PostMessageA(hwnd, WM_KEYDOWN, VK_RETURN, 0);
        PostMessageA(hwnd, WM_KEYUP, VK_RETURN, 0);
    }
    return TRUE;
}

int main(int argc, char **argv)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char commandLine[1024];
    HWND optionCreate, runButton, textBoxes[8];
    char readBack[512];
    int textBoxCount, i, waited;

    if (argc < 6) {
        printf("usage: driver <exe> <sourcePath> <targetPath> <patchPath> <modeText>\n");
        return 1;
    }
    snprintf(commandLine, sizeof(commandLine), "%s", argv[1]);

    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessA(NULL, commandLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf("CreateProcess failed: %lu\n", (unsigned long)GetLastError());
        return 1;
    }
    targetProcessId = pi.dwProcessId;
    WaitForInputIdle(pi.hProcess, 15000);
    Sleep(LAUNCH_SETTLE_MS);

    EnumWindows(findForm, 0);
    if (!formWindow) { printf("form window not found\n"); TerminateProcess(pi.hProcess, 0); return 1; }
    printf("form hwnd = %p\n", (void *)formWindow);

    refreshControls();
    dumpControls("initial");

    optionCreate = findByText("OptionButton", argv[5]);
    if (!optionCreate) { printf("mode option \"%s\" not found\n", argv[5]); TerminateProcess(pi.hProcess, 0); return 1; }
    printf("clicking mode option hwnd=%p (\"%s\")\n", (void *)optionCreate, argv[5]);
    SetFocus(optionCreate);
    SendMessageA(optionCreate, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageA(optionCreate, BM_CLICK, 0, 0);
    Sleep(MODE_SETTLE_MS);

    refreshControls();
    dumpControls("after mode click");

    textBoxCount = collectTextBoxes(textBoxes, 8);
    printf("visible textboxes (top to bottom): %d\n", textBoxCount);
    for (i = 0; i < textBoxCount && i < 3; i++) {
        const char *value = argv[2 + i];
        SetFocus(textBoxes[i]);
        SendMessageA(textBoxes[i], WM_SETTEXT, 0, (LPARAM)value);
        Sleep(FIELD_SETTLE_MS);
        readBack[0] = 0;
        SendMessageA(textBoxes[i], WM_GETTEXT, sizeof(readBack) - 1, (LPARAM)readBack);
        printf("  textbox[%d] hwnd=%p set=\"%s\" readback=\"%s\"\n",
               i, (void *)textBoxes[i], value, readBack);
    }
    fflush(stdout);
    Sleep(BEFORE_RUN_MS);

    runButton = findByText("CommandButton", "Run");
    if (!runButton) { printf("Run button not found\n"); TerminateProcess(pi.hProcess, 0); return 1; }
    printf("clicking Run hwnd=%p\n", (void *)runButton);
    fflush(stdout);
    SetFocus(runButton);
    SendMessageA(runButton, BM_CLICK, 0, 0);

    for (waited = 0; waited < RESULT_POLL_LIMIT; waited++) {
        Sleep(RESULT_POLL_MS);
        EnumWindows(reportPopup, 0);
    }

    printf("done, terminating\n");
    fflush(stdout);
    TerminateProcess(pi.hProcess, 0);
    return 0;
}
