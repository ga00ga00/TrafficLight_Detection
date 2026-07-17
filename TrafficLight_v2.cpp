//TrafficLight_v2
//this is real (if)
//Use claude AI
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;

struct Args {
    std::string camera = "2";
    std::string rotate = "none";
    std::array<double, 4> roi{0.0, 0.10, 1.0, 0.65};
    int width = 640;
    int height = 480;
    int confirm = 3;
    bool printEveryFrame = false;

    std::string saveDir = "~/Downloads/traffic_light_frames";
    std::string saveFormat = "jpg";
    int jpegQuality = 95;
    int saveEveryN = 1;
    bool saveDebug = false;
    bool saveOnlyDetected = false;
    bool allowPortrait = false;

    bool hasManualExposure = false;
    double manualExposure = 0.0;

    int rectWidth = 48;
    int rectHeight = 28;
    int whiteV = 235;
    int whiteS = 45;
    int whiteRgbMin = 230;
    int whiteRgbDiff = 45;
    int activeV = 130;
    int activeS = 35;
    int activeRgbSpread = 25;
    int validV = 30;
    int minValidPixels = 20;
    double minWhiteArea = 12.0;
    double maxWhiteAreaRatio = 0.20;
    double minLightCircularity = 0.02;
    double minLightAspect = 0.25;
    double maxLightAspect = 4.0;

    double lampClusterRadiusMin = 32.0;
    double lampClusterRadiusScale = 2.2;
    double lampClusterYScale = 0.9;
    double lightBodyYMarginScale = 0.35;
    double lightBodyXLeftMarginScale = 0.35;
    double lightBodyXRightMarginScale = 1.20;

    double greenRGap = 10.0;
    double greenBGap = 3.0;
    double redGGap = 20.0;
    //double yellowBGap = 18.0;
    //#
    double yellowBGapHigh = 6.0;   // N/R 상태에서 O로 "진입"하는 문턱, 링 샘플링 실측: 빨강 G-B<=-1(rect 기준), 주황 G-B>=11 → 중간값
    double yellowBGapLow  = 6.0;    // O 상태에서 O를 "유지"하는 문턱
    double yellowGbRgRatio = 0.12;  // 링 샘플링 실측: 빨강 비율~0, 주황 비율>=0.19 → 중간값
    int orangeMinS = 0;
    int minColorS = 25;             // [⑤] 이보다 채도가 낮으면 무채색으로 보고 'N' (실측: 주황 S>=49, 빨강 S>=125)
    double glowRingScale = 1.6;     // [③] 포화 코어일 때 반지름 r ~ r*scale 링(글로우)에서 색을 재판정

    int darkV = 115;
    int minBodyWidth = 45;
    int minBodyHeight = 12;
    double maxBodyAreaRatio = 0.5;   // 0.35 → 0.5: 근거리에서 하우징이 커져도 body 후보 유지
    double bodyMinAspect = 1.35;
    double bodyMaxAspect = 12.0;
    double maxLightBodyGapScale = 3.5;
    double minPairVerticalOverlap = 0.05;

    int trafficPadX = 6;
    int trafficPadY = 6;
    int minTrafficWidth = 60;
    int minTrafficHeight = 25;
    double trafficMinAspect = 1.4;
    double trafficMaxAspect = 10.0;

    double bboxSmoothAlpha = 0.70;
    int bboxMaxMissing = 8;

    //범위를 안 보고싶다면 showSearchRoi = false;
    bool showSearchRoi = true;
    bool showMask = false;
    bool showCandidates = false;
    bool showBodyCandidates = false;
    bool showPairs = false;
    bool showTrafficBbox = false;

    double redAnchorConfidence = 22.0;
    double orangeAnchorConfidence = 18.0;
    double greenAnchorConfidence = 35.0;
    bool disableColorOverride = false;

    double fallbackRedConfidence = 24.0;
    double fallbackOrangeConfidence = 20.0;
    double fallbackGreenConfidence = 40.0;
    bool disableColorFallback = false;

    double estimatedTrafficAspect = 4.6;
    double estimatedTrafficHeightScale = 1.25;
};

static std::string expandUserPath(const std::string& path) {
    if (!path.empty() && path[0] == '~') {
        const char* home = std::getenv("HOME");
        if (home) return std::string(home) + path.substr(1);
    }
    return path;
}

static std::string nowString(const char* fmt) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, fmt);
    return oss.str();
}

static std::string nowStringWithMs() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S_") << std::setw(3) << std::setfill('0') << ms.count();
    return oss.str();
}

static bool isNumber(const std::string& s) {
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
}

static void printHelp() {
    std::cout
        << "Usage: ./CameraOne_landscape_save_frames [options]\n"
        << "  --camera N|PATH\n"
        << "  --rotate none|cw|ccw|180\n"
        << "  --width N --height N\n"
        << "  --save-dir PATH\n"
        << "  --save-format jpg|png\n"
        << "  --jpeg-quality 1..100\n"
        << "  --save-every-n N\n"
        << "  --save-debug\n"
        << "  --save-only-detected\n"
        << "  --allow-portrait\n"
        << "  --print-every-frame\n"
        << "  --show-mask --show-candidates --show-body-candidates\n"
        << "  --show-pairs --show-search-roi --show-traffic-bbox\n"
        << "  --manual-exposure VALUE\n";
}

static Args parseArgs(int argc, char** argv) {
    Args a;
    auto need = [&](int& i) -> std::string {
        if (i + 1 >= argc) throw std::runtime_error(std::string("missing value after ") + argv[i]);
        return argv[++i];
    };

    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        if (k == "--help" || k == "-h") { printHelp(); std::exit(0); }
        else if (k == "--camera") a.camera = need(i);
        else if (k == "--rotate") a.rotate = need(i);
        else if (k == "--width") a.width = std::stoi(need(i));
        else if (k == "--height") a.height = std::stoi(need(i));
        else if (k == "--confirm") a.confirm = std::stoi(need(i));
        else if (k == "--save-dir") a.saveDir = need(i);
        else if (k == "--save-format") a.saveFormat = need(i);
        else if (k == "--jpeg-quality") a.jpegQuality = std::stoi(need(i));
        else if (k == "--save-every-n") a.saveEveryN = std::stoi(need(i));
        else if (k == "--manual-exposure") { a.hasManualExposure = true; a.manualExposure = std::stod(need(i)); }
        else if (k == "--roi") {
            for (double& v : a.roi) v = std::stod(need(i));
        }
        else if (k == "--print-every-frame") a.printEveryFrame = true;
        else if (k == "--save-debug") a.saveDebug = true;
        else if (k == "--save-only-detected") a.saveOnlyDetected = true;
        else if (k == "--allow-portrait") a.allowPortrait = true;
        else if (k == "--show-search-roi") a.showSearchRoi = true;
        else if (k == "--show-mask") a.showMask = true;
        else if (k == "--show-candidates") a.showCandidates = true;
        else if (k == "--show-body-candidates") a.showBodyCandidates = true;
        else if (k == "--show-pairs") a.showPairs = true;
        else if (k == "--show-traffic-bbox") a.showTrafficBbox = true;
        else if (k == "--disable-color-override") a.disableColorOverride = true;
        else if (k == "--disable-color-fallback") a.disableColorFallback = true;
        else std::cerr << "[WARN] unknown option ignored: " << k << '\n';
    }
    a.saveEveryN = std::max(1, a.saveEveryN);
    a.jpegQuality = std::clamp(a.jpegQuality, 1, 100);
    return a;
}

struct ColorInfo {
    double meanR = 0, meanG = 0, meanB = 0;
    double meanH = 0, meanS = 0, meanV = 0;
    double rg = 0, gb = 0;
    int validPixels = 0;
    int whitePixels = 0;
    double confidence = 0;
};

struct LightCandidate {
    cv::Rect blobBbox;
    cv::Rect rectBbox;
    cv::Point2f center;
    double area = 0;
    double circularity = 0;
    double aspect = 0;
    int whitePixels = 0;
    int activePixels = 0;
    double score = 0;
    char colorLabel = 'N';
    ColorInfo colorInfo;
    int clusterMemberCount = 1;
};

struct BodyCandidate {
    cv::Rect bbox;
    double score = 0;
    double fillRatio = 0;
    double aspect = 0;
};

struct PairCandidate {
    cv::Rect trafficBbox;
    LightCandidate light;
    BodyCandidate body;
    double score = 0;
    char relativeLabel = 'N';
    int slotIdx = -1;
    double relativeX = 0;
};

struct DetectionInfo {
    cv::Rect roiRect;
    std::optional<cv::Rect> trafficBbox;
    std::string bboxSource = "none";
    std::optional<LightCandidate> selectedLight;
    std::optional<BodyCandidate> selectedBody;
    std::vector<LightCandidate> lightCandidates;
    std::vector<BodyCandidate> bodyCandidates;
    std::vector<PairCandidate> pairCandidates;
    std::vector<cv::Point2f> slotCenters;
    cv::Mat whiteMask, activeMask, darkMask;
};

class SafeStableLabel {
public:
    explicit SafeStableLabel(int size) : size_(std::max(1, size)) {}
    char update(char value) {
        if (value == candidate_) count_++;
        else { candidate_ = value; count_ = 1; }
        // G는 기존대로 size_ 프레임, R/O/N은 2프레임 연속이면 확정
        int need = (value == 'G') ? size_ : 2;
        if (count_ >= need) stable_ = candidate_;
        return stable_;
    }
private:
    int size_ = 3;
    int count_ = 0;
    char candidate_ = 'N';
    char stable_ = 'N';
};

class BBoxTracker {
public:
    BBoxTracker(double alpha, int maxMissing)
        : alpha_(std::clamp(alpha, 0.0, 1.0)), maxMissing_(maxMissing) {}

    std::pair<std::optional<cv::Rect>, std::string> update(const std::optional<cv::Rect>& bbox) {
        if (!bbox) {
            missing_++;
            if (has_ && missing_ <= maxMissing_) {
                return {cv::Rect(cvRound(state_[0]), cvRound(state_[1]), cvRound(state_[2]), cvRound(state_[3])), "tracker_hold"};
            }
            has_ = false;
            return {std::nullopt, "none"};
        }
        cv::Vec4f n((float)bbox->x, (float)bbox->y, (float)bbox->width, (float)bbox->height);
        if (!has_ || missing_ > maxMissing_) state_ = n;
        else state_ = (float)alpha_ * state_ + (float)(1.0 - alpha_) * n;
        missing_ = 0;
        has_ = true;
        return {cv::Rect(cvRound(state_[0]), cvRound(state_[1]), cvRound(state_[2]), cvRound(state_[3])), "tracked"};
    }
private:
    double alpha_;
    int maxMissing_;
    int missing_ = 0;
    bool has_ = false;
    cv::Vec4f state_{};
};

static cv::Mat rotateFrame(const cv::Mat& frame, const std::string& mode) {
    cv::Mat out;
    if (mode == "cw") cv::rotate(frame, out, cv::ROTATE_90_CLOCKWISE);
    else if (mode == "ccw") cv::rotate(frame, out, cv::ROTATE_90_COUNTERCLOCKWISE);
    else if (mode == "180") cv::rotate(frame, out, cv::ROTATE_180);
    else out = frame.clone();
    return out;
}

static cv::Mat ensureLandscape(const cv::Mat& frame) {
    if (frame.rows > frame.cols) {
        cv::Mat out; cv::rotate(frame, out, cv::ROTATE_90_CLOCKWISE); return out;
    }
    return frame;
}

static cv::Rect clampRect(double x, double y, double w, double h, const cv::Size& s) {
    int ix = cvRound(x), iy = cvRound(y), iw = std::max(1, cvRound(w)), ih = std::max(1, cvRound(h));
    if (ix < 0) { iw += ix; ix = 0; }
    if (iy < 0) { ih += iy; iy = 0; }
    if (ix + iw > s.width) iw = s.width - ix;
    if (iy + ih > s.height) ih = s.height - iy;
    if (iw <= 0 || ih <= 0) return {};
    return {ix, iy, iw, ih};
}

static cv::Rect rectFromCenter(double cx, double cy, int w, int h, const cv::Size& s) {
    return clampRect(cx - w / 2.0, cy - h / 2.0, w, h, s);
}

static cv::Rect unionRect(const cv::Rect& a, const cv::Rect& b, int px, int py, const cv::Size& s) {
    int x1 = std::min(a.x, b.x) - px;
    int y1 = std::min(a.y, b.y) - py;
    int x2 = std::max(a.x + a.width, b.x + b.width) + px;
    int y2 = std::max(a.y + a.height, b.y + b.height) + py;
    return clampRect(x1, y1, x2 - x1, y2 - y1, s);
}

static double overlap1d(double a1, double a2, double b1, double b2) {
    return std::max(0.0, std::min(a2, b2) - std::max(a1, b1));
}

static cv::Mat makeWhiteMask(const cv::Mat& bgr, const Args& a) {
    cv::Mat hsv; cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);
    cv::Mat mask(bgr.size(), CV_8U, cv::Scalar(0));
    for (int y = 0; y < bgr.rows; ++y) {
        const auto* bp = bgr.ptr<cv::Vec3b>(y);
        const auto* hp = hsv.ptr<cv::Vec3b>(y);
        auto* mp = mask.ptr<uchar>(y);
        for (int x = 0; x < bgr.cols; ++x) {
            int B = bp[x][0], G = bp[x][1], R = bp[x][2];
            int maxv = std::max({B, G, R}), minv = std::min({B, G, R});
            mp[x] = (hp[x][2] >= a.whiteV && hp[x][1] <= a.whiteS && maxv >= a.whiteRgbMin && maxv - minv <= a.whiteRgbDiff) ? 255 : 0;
        }
    }
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, cv::Mat::ones(3, 3, CV_8U));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, cv::Mat::ones(5, 5, CV_8U));
    return mask;
}

static cv::Mat makeActiveMask(const cv::Mat& bgr, const Args& a) {
    cv::Mat hsv; cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);
    cv::Mat white = makeWhiteMask(bgr, a);
    cv::Mat mask(bgr.size(), CV_8U, cv::Scalar(0));
    for (int y = 0; y < bgr.rows; ++y) {
        const auto* bp = bgr.ptr<cv::Vec3b>(y);
        const auto* hp = hsv.ptr<cv::Vec3b>(y);
        const auto* wp = white.ptr<uchar>(y);
        auto* mp = mask.ptr<uchar>(y);
        for (int x = 0; x < bgr.cols; ++x) {
            int B = bp[x][0], G = bp[x][1], R = bp[x][2];
            int spread = std::max({B, G, R}) - std::min({B, G, R});
            bool bright = hp[x][2] >= a.activeV && (hp[x][1] >= a.activeS || spread >= a.activeRgbSpread);
            mp[x] = (wp[x] || bright) ? 255 : 0;
        }
    }
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, cv::Mat::ones(3, 3, CV_8U));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, cv::Mat::ones(5, 5, CV_8U));
    cv::dilate(mask, mask, cv::Mat::ones(3, 3, CV_8U));
    return mask;
}

static cv::Mat makeDarkMask(const cv::Mat& bgr, const Args& a) {
    cv::Mat hsv; cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);
    cv::Mat mask;
    cv::inRange(hsv, cv::Scalar(0, 0, 0), cv::Scalar(179, 255, a.darkV), mask);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, cv::Mat::ones(7, 7, CV_8U));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, cv::Mat::ones(3, 3, CV_8U));
    return mask;
}

// extraMask가 주어지면(non-empty) 해당 마스크가 켜진 픽셀만 사용한다. (classifyRing에서 사용)
static std::pair<char, ColorInfo> classifyRect(const cv::Mat& rect, const Args& a,
                                               const cv::Mat& extraMask = cv::Mat()) {
    ColorInfo info;
    if (rect.empty()) return {'N', info};
    cv::Mat hsv; cv::cvtColor(rect, hsv, cv::COLOR_BGR2HSV);
    cv::Mat white = makeWhiteMask(rect, a);

    double sb = 0, sg = 0, sr = 0, sh = 0, ss = 0, sv = 0;
    int count = 0, whiteCount = cv::countNonZero(white);
    auto accumulate = [&](bool requireV) {
        sb = sg = sr = sh = ss = sv = 0; count = 0;
        for (int y = 0; y < rect.rows; ++y) {
            const auto* bp = rect.ptr<cv::Vec3b>(y);
            const auto* hp = hsv.ptr<cv::Vec3b>(y);
            const auto* wp = white.ptr<uchar>(y);
            const uchar* ep = extraMask.empty() ? nullptr : extraMask.ptr<uchar>(y);
            for (int x = 0; x < rect.cols; ++x) {
                if (ep && !ep[x]) continue;   // 링 마스크 밖 픽셀 제외
                if (wp[x]) continue;
                if (requireV && hp[x][2] < a.validV) continue;
                sb += bp[x][0]; sg += bp[x][1]; sr += bp[x][2];
                sh += hp[x][0]; ss += hp[x][1]; sv += hp[x][2]; count++;
            }
        }
    };
    accumulate(true);
    if (count < a.minValidPixels) accumulate(false);
    info.validPixels = count;
    info.whitePixels = whiteCount;
    if (count < a.minValidPixels) return {'N', info};

    info.meanB = sb / count; info.meanG = sg / count; info.meanR = sr / count;
    info.meanH = sh / count; info.meanS = ss / count; info.meanV = sv / count;
    info.rg = info.meanR - info.meanG; info.gb = info.meanG - info.meanB;

    // [⑤] 무채색 컷: 채도가 낮으면 색 판정 자체를 하지 않는다 (차선/천장 트러스 등 오탐 차단)
    if (info.meanS < a.minColorS) return {'N', info};

    char label = 'N';
    if (info.meanG > info.meanR + a.greenRGap && info.meanG > info.meanB + a.greenBGap) {
        label = 'G'; info.confidence = (info.meanG - info.meanR) + (info.meanG - info.meanB);
    } else if (info.meanR > info.meanG + a.redGGap) {
        bool gapOk   = info.meanG > info.meanB + a.yellowBGapHigh;      // 실측: 빨강 G-B<=-1, 주황 G-B>=15
        bool ratioOk = info.gb > a.yellowGbRgRatio * info.rg;           // 비율 조건 AND (노출 변화 보험)
        if (gapOk && ratioOk && info.meanS >= a.orangeMinS) {
            label = 'O'; info.confidence = (info.meanR - info.meanG) + (info.meanG - info.meanB);
        } else {
            label = 'R'; info.confidence = info.meanR - std::max(info.meanG, info.meanB);
        }
    }
    return {label, info};
}

// [③] 포화 코어 대응: 코어(반지름 r) 바깥의 글로우 링(r ~ r*glowRingScale)에서만 색을 판정한다.
static std::pair<char, ColorInfo> classifyRing(const cv::Mat& roi, const cv::Point2f& center,
                                               double radius, const Args& a) {
    double r2 = std::max(radius * a.glowRingScale, radius + 4.0);   // 링 두께 최소 4px 확보
    cv::Rect box = clampRect(center.x - r2, center.y - r2, 2.0 * r2, 2.0 * r2, roi.size());
    if (box.empty()) return {'N', ColorInfo{}};
    cv::Mat ring(box.size(), CV_8U, cv::Scalar(0));
    cv::Point c(cvRound(center.x - box.x), cvRound(center.y - box.y));
    cv::circle(ring, c, cvRound(r2), cv::Scalar(255), cv::FILLED);   // 바깥 원 채우고
    cv::circle(ring, c, cvRound(radius), cv::Scalar(0), cv::FILLED); // 코어를 파내면 링만 남는다
    return classifyRect(roi(box), a, ring);
}

static std::vector<LightCandidate> findLights(const cv::Mat& roi, const Args& a, cv::Mat& white, cv::Mat& active) {
    white = makeWhiteMask(roi, a);
    active = makeActiveMask(roi, a);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(white.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    std::vector<LightCandidate> out;
    double maxArea = roi.total() * a.maxWhiteAreaRatio;
    for (const auto& c : contours) {
        double area = cv::contourArea(c);
        if (area < a.minWhiteArea || area > maxArea) continue;
        cv::Rect b = cv::boundingRect(c);
        if (b.width <= 1 || b.height <= 1) continue;
        double aspect = b.width / (double)b.height;
        if (aspect < a.minLightAspect || aspect > a.maxLightAspect) continue;
        double per = cv::arcLength(c, true);
        double circ = per > 0 ? 4.0 * CV_PI * area / (per * per) : 0;
        if (circ < a.minLightCircularity) continue;
        cv::Point2f center(b.x + b.width / 2.0f, b.y + b.height / 2.0f);
        cv::Rect rb = rectFromCenter(center.x, center.y, a.rectWidth, a.rectHeight, roi.size());
        if (rb.empty()) continue;
        auto [label, ci] = classifyRect(roi(rb), a);
        // [③] rect의 절반 이상이 포화(white)라 색을 못 읽었다면 → 글로우 링에서 재판정
        if (label == 'N' && ci.whitePixels > rb.area() / 2) {
            cv::Point2f cc; float radius;
            cv::minEnclosingCircle(c, cc, radius);   // contour로부터 코어 원 추정
            auto [l2, ci2] = classifyRing(roi, cc, radius, a);
            if (l2 != 'N') { label = l2; ci = ci2; }
        }

        cv::Mat contourMask(roi.size(), CV_8U, cv::Scalar(0));
        std::vector<std::vector<cv::Point>> one{c};
        cv::drawContours(contourMask, one, -1, cv::Scalar(255), cv::FILLED);
        cv::Mat tmp;
        cv::bitwise_and(white, contourMask, tmp);
        int wp = cv::countNonZero(tmp);
        cv::bitwise_and(active, contourMask, tmp);
        int ap = cv::countNonZero(tmp);

        LightCandidate lc;
        lc.blobBbox = b; lc.rectBbox = rb; lc.center = center; lc.area = area; lc.circularity = circ;
        lc.aspect = aspect; lc.whitePixels = wp; lc.activePixels = ap; lc.colorLabel = label; lc.colorInfo = ci;
        lc.score = wp * 2.0 + ap * 0.2 + circ * 40.0 + ci.confidence * 0.3;
        out.push_back(lc);
    }
    std::sort(out.begin(), out.end(), [](auto& x, auto& y){ return x.score > y.score; });
    return out;
}

static std::vector<BodyCandidate> findBodies(const cv::Mat& roi, const Args& a, cv::Mat& dark) {
    dark = makeDarkMask(roi, a);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(dark.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    std::vector<BodyCandidate> out;
    double maxArea = roi.total() * a.maxBodyAreaRatio;
    for (const auto& c : contours) {
        cv::Rect b = cv::boundingRect(c);
        double rectArea = (double)b.area();
        double aspect = b.width / (double)std::max(1, b.height);
        if (b.width < a.minBodyWidth || b.height < a.minBodyHeight || rectArea > maxArea) continue;
        if (aspect < a.bodyMinAspect || aspect > a.bodyMaxAspect) continue;
        double fill = cv::contourArea(c) / std::max(1.0, rectArea);
        BodyCandidate bc; bc.bbox = b; bc.fillRatio = fill; bc.aspect = aspect;
        bc.score = rectArea * 0.01 + fill * 25.0 + aspect * 3.0;
        out.push_back(bc);
    }
    std::sort(out.begin(), out.end(), [](auto& x, auto& y){ return x.score > y.score; });
    return out;
}

static char slotToLabel(int i) { return i <= 0 ? 'R' : (i == 1 ? 'O' : 'G'); }
static int labelToSlot(char c) { return c == 'R' ? 0 : (c == 'O' ? 1 : (c == 'G' ? 2 : -1)); }

static std::tuple<char,int,double> assignByPosition(const cv::Point2f& p, const cv::Rect& b) {
    if (b.width <= 0 || b.height <= 0) return {'N', -1, 0};
    double rx = (p.x - b.x) / b.width;
    if (rx < 0 || rx > 1) return {'N', -1, rx};
    if (rx < 1.0/3.0) return {'R', 0, rx};
    if (rx < 2.0/3.0) return {'O', 1, rx};
    return {'G', 2, rx};
}

static bool plausible(const LightCandidate& l, const BodyCandidate& b, const Args& a) {
    double bh = std::max(1, b.bbox.height);
    double x1 = b.bbox.x - bh * a.lightBodyXLeftMarginScale;
    double x2 = b.bbox.x + b.bbox.width + bh * a.lightBodyXRightMarginScale;
    double y1 = b.bbox.y - bh * a.lightBodyYMarginScale;
    double y2 = b.bbox.y + b.bbox.height + bh * a.lightBodyYMarginScale;
    if (l.center.x < x1 || l.center.x > x2 || l.center.y < y1 || l.center.y > y2) return false;
    double bodyCy = b.bbox.y + b.bbox.height / 2.0;
    double yGapNorm = std::abs(l.center.y - bodyCy) / bh;
    if (yGapNorm > 0.5 + a.lightBodyYMarginScale) return false;
    double ov = overlap1d(l.blobBbox.y, l.blobBbox.y + l.blobBbox.height, b.bbox.y, b.bbox.y + b.bbox.height);
    double ratio = ov / std::max(1.0, std::min((double)l.blobBbox.height, bh));
    return !(ratio < a.minPairVerticalOverlap && yGapNorm > 0.55);
}

static LightCandidate buildCluster(const LightCandidate& seed, const std::vector<LightCandidate>& lights,
                                   const cv::Mat& roi, const std::optional<BodyCandidate>& body, const Args& a) {
    double radius = std::max(a.lampClusterRadiusMin, std::max(seed.blobBbox.width, seed.blobBbox.height) * a.lampClusterRadiusScale);
    double yr = radius * a.lampClusterYScale;
    std::vector<const LightCandidate*> members;
    for (const auto& c : lights) {
        if (std::abs(c.center.x - seed.center.x) > radius || std::abs(c.center.y - seed.center.y) > yr) continue;
        if (body && !plausible(c, *body, a)) continue;
        if (cv::norm(c.center - seed.center) > radius) continue;
        members.push_back(&c);
    }
    if (members.empty()) members.push_back(&seed);

    double sw = 0, sx = 0, sy = 0, area = 0, memberScore = 0;
    int x1 = roi.cols, y1 = roi.rows, x2 = 0, y2 = 0, wp = 0, ap = 0;
    for (auto* c : members) {
        double w = std::max(1.0, c->whitePixels + c->area);
        sw += w; sx += c->center.x * w; sy += c->center.y * w;
        x1 = std::min(x1, c->blobBbox.x); y1 = std::min(y1, c->blobBbox.y);
        x2 = std::max(x2, c->blobBbox.x + c->blobBbox.width); y2 = std::max(y2, c->blobBbox.y + c->blobBbox.height);
        wp += c->whitePixels; ap += c->activePixels; area += c->area; memberScore += c->score;
    }
    LightCandidate out = seed;
    out.center = cv::Point2f((float)(sx/sw), (float)(sy/sw));
    out.blobBbox = clampRect(x1, y1, x2-x1, y2-y1, roi.size());
    out.rectBbox = rectFromCenter(out.center.x, out.center.y, a.rectWidth, a.rectHeight, roi.size());
    auto [label, ci] = classifyRect(roi(out.rectBbox), a);
    // [③] 클러스터 rect가 포화 코어에 앉은 경우: blob 크기에서 반지름을 추정해 링 재판정
    if (label == 'N' && ci.whitePixels > out.rectBbox.area() / 2) {
        double radius = std::max(out.blobBbox.width, out.blobBbox.height) / 2.0;
        auto [l2, ci2] = classifyRing(roi, out.center, radius, a);
        if (l2 != 'N') { label = l2; ci = ci2; }
    }
    out.colorLabel = label; out.colorInfo = ci; out.whitePixels = wp; out.activePixels = ap; out.area = area;
    out.clusterMemberCount = (int)members.size();
    out.score = memberScore + 35.0 * std::max(0, out.clusterMemberCount - 1);
    return out;
}

static std::optional<PairCandidate> scorePair(const LightCandidate& l, const BodyCandidate& b, const cv::Mat& roi, const Args& a) {
    if (!plausible(l, b, a)) return std::nullopt;
    double bh = std::max(1, b.bbox.height);
    double ov = overlap1d(l.blobBbox.y, l.blobBbox.y+l.blobBbox.height, b.bbox.y, b.bbox.y+b.bbox.height);
    double ovRatio = ov / std::max(1.0, std::min((double)l.blobBbox.height, bh));
    double bodyCy = b.bbox.y + b.bbox.height / 2.0;
    double yGap = std::abs(l.center.y - bodyCy) / bh;
    double xGap = 0;
    if (l.blobBbox.x + l.blobBbox.width < b.bbox.x) xGap = b.bbox.x - (l.blobBbox.x + l.blobBbox.width);
    else if (b.bbox.x + b.bbox.width < l.blobBbox.x) xGap = l.blobBbox.x - (b.bbox.x + b.bbox.width);
    double gapNorm = xGap / bh;
    if (gapNorm > a.maxLightBodyGapScale) return std::nullopt;
    cv::Rect u = unionRect(b.bbox, l.blobBbox, a.trafficPadX, a.trafficPadY, roi.size());
    if (u.empty() || u.width < a.minTrafficWidth || u.height < a.minTrafficHeight) return std::nullopt;
    double aspect = u.width / (double)u.height;
    if (aspect < a.trafficMinAspect || aspect > a.trafficMaxAspect) return std::nullopt;
    auto [rel, slot, rx] = assignByPosition(l.center, u);
    if (rel == 'N') return std::nullopt;
    double colorBonus = 0;
    if (l.colorLabel == rel && l.colorLabel != 'N') colorBonus = 160 + std::min(120.0, l.colorInfo.confidence * 2.0);
    else if (l.colorLabel == 'O') colorBonus = 120 + std::min(100.0, l.colorInfo.confidence * 1.5);
    else if (l.colorLabel != 'N') colorBonus = 30;
    PairCandidate p; p.trafficBbox = u; p.light = l; p.body = b; p.relativeLabel = rel; p.slotIdx = slot; p.relativeX = rx;
    p.score = l.whitePixels*2.2 + l.area*0.65 + l.clusterMemberCount*45.0 + l.score*0.35 + b.score*1.6
            + ovRatio*150.0 + std::max(0.0, 1.0-yGap)*180.0 + colorBonus - gapNorm*35.0 - std::max(0.0, yGap-0.35)*220.0;
    return p;
}

static double anchorThreshold(char c, const Args& a) {
    return c=='R'?a.redAnchorConfidence:(c=='O'?a.orangeAnchorConfidence:(c=='G'?a.greenAnchorConfidence:1e9));
}
static double fallbackThreshold(char c, const Args& a) {
    return c=='R'?a.fallbackRedConfidence:(c=='O'?a.fallbackOrangeConfidence:(c=='G'?a.fallbackGreenConfidence:1e9));
}
static bool confident(const LightCandidate& l, const Args& a, bool fallback=false) {
    if (l.colorLabel!='R' && l.colorLabel!='O' && l.colorLabel!='G') return false;
    return l.colorInfo.confidence >= (fallback ? fallbackThreshold(l.colorLabel,a) : anchorThreshold(l.colorLabel,a));
}

static std::optional<cv::Rect> estimateTraffic(const LightCandidate& l, const std::optional<BodyCandidate>& b,
                                                const cv::Mat& roi, const Args& a) {
    int slot = labelToSlot(l.colorLabel);
    if (slot < 0) return std::nullopt;
    double bh = b ? b->bbox.height : 0.0;
    double bcy = b ? b->bbox.y + b->bbox.height/2.0 : l.center.y;
    double th = std::max({(double)a.minTrafficHeight, bh*a.estimatedTrafficHeightScale,
                          l.blobBbox.height*2.4, a.rectHeight*1.25});
    double tw = std::max((double)a.minTrafficWidth, th*a.estimatedTrafficAspect);
    double ratio = (2.0*slot+1.0)/6.0;
    return clampRect(l.center.x - tw*ratio, bcy-th/2.0, tw, th, roi.size());
}

class TrafficLightDetector {
public:
    explicit TrafficLightDetector(const Args& a) : a_(a), tracker_(a.bboxSmoothAlpha, a.bboxMaxMissing) {}

    std::pair<char, DetectionInfo> detect(const cv::Mat& frame) {
        DetectionInfo info;
        int x1 = cvRound(std::clamp(a_.roi[0],0.0,1.0)*frame.cols);
        int y1 = cvRound(std::clamp(a_.roi[1],0.0,1.0)*frame.rows);
        int x2 = cvRound(std::clamp(a_.roi[2],0.0,1.0)*frame.cols);
        int y2 = cvRound(std::clamp(a_.roi[3],0.0,1.0)*frame.rows);
        if (x2<=x1 || y2<=y1) { x1=0; y1=0; x2=frame.cols; y2=frame.rows; }
        info.roiRect = {x1,y1,x2-x1,y2-y1};
        cv::Mat roi = frame(info.roiRect);

        auto lights = findLights(roi, a_, info.whiteMask, info.activeMask);
        auto bodies = findBodies(roi, a_, info.darkMask);
        std::vector<PairCandidate> pairs;
        std::set<std::tuple<int,int,int,int>> keys;
        for (size_t bi=0; bi<std::min<size_t>(14,bodies.size()); ++bi) {
            for (size_t li=0; li<std::min<size_t>(20,lights.size()); ++li) {
                if (!plausible(lights[li], bodies[bi], a_)) continue;
                auto cluster = buildCluster(lights[li], lights, roi, bodies[bi], a_);
                auto key = std::make_tuple(bodies[bi].bbox.x/8,bodies[bi].bbox.y/8,cvRound(cluster.center.x/12),cvRound(cluster.center.y/12));
                if (!keys.insert(key).second) continue;
                auto p = scorePair(cluster, bodies[bi], roi, a_);
                if (p) pairs.push_back(*p);
            }
        }
        std::sort(pairs.begin(), pairs.end(), [](auto& x, auto& y){ return x.score > y.score; });

        std::optional<cv::Rect> raw;
        std::optional<LightCandidate> selectedLight;
        std::optional<BodyCandidate> selectedBody;
        std::string source = "none";

        if (!pairs.empty()) {
            raw = pairs[0].trafficBbox; selectedLight = pairs[0].light; selectedBody = pairs[0].body;
            source = "body_white_cluster_union";
            if (!a_.disableColorOverride && confident(*selectedLight, a_)) {
                auto anchored = estimateTraffic(*selectedLight, selectedBody, roi, a_);
                if (anchored) { raw = anchored; source = std::string(1,selectedLight->colorLabel)+"_color_anchor"; }
            }
        }

        if (!raw && !a_.disableColorFallback) {
            double bestScore=-1e18; std::optional<LightCandidate> best;
            for (const auto& l: lights) {
                if (!confident(l,a_,true)) continue;
                double s=l.colorInfo.confidence*4+l.whitePixels*0.8+l.area*0.6+l.score*0.25+(l.colorLabel=='R'?80:(l.colorLabel=='O'?50:0));
                if (s>bestScore) { bestScore=s; best=l; }
            }
            if (best) {
                selectedLight=best; selectedBody.reset(); raw=estimateTraffic(*best,std::nullopt,roi,a_);
                source=std::string(1,best->colorLabel)+"_color_fallback";
            }
        }

        auto [smoothLocal,state] = tracker_.update(raw);
        if (!raw && smoothLocal) source=state;

        /*char label='N'; int slot=-1; double rx=0;
        if (selectedLight && smoothLocal) {
            std::tie(label,slot,rx)=assignByPosition(selectedLight->center,*smoothLocal);
            if (!a_.disableColorOverride && confident(*selectedLight,a_)) {
                int cs=labelToSlot(selectedLight->colorLabel);
                if (cs>=0) { label=selectedLight->colorLabel; slot=cs; }
            }
        }*/

        char label='N'; int slot=-1; double rx=0;
        if (selectedLight && smoothLocal) {
            std::tie(label,slot,rx)=assignByPosition(selectedLight->center,*smoothLocal);
            if (!a_.disableColorOverride && confident(*selectedLight,a_)) {
                int cs=labelToSlot(selectedLight->colorLabel);
                if (cs>=0) { label=selectedLight->colorLabel; slot=cs; }
            }
            // [SAFETY GUARD] 위치만으로 G를 확정하지 않는다.
            // 색 라벨이 G이고 confidence까지 통과한 경우에만 G 허용.
            if (label=='G' && !(selectedLight->colorLabel=='G' && confident(*selectedLight,a_))) {
                label='N'; slot=-1;
            }
            // [④] 색 증거가 전혀 없는 후보('N')는 위치만으로 R/O도 확정하지 않는다
            if (selectedLight->colorLabel=='N') { label='N'; slot=-1; }
        }

        auto addOffsetRect=[&](const cv::Rect& r){ return cv::Rect(r.x+x1,r.y+y1,r.width,r.height); };
        auto addOffsetLight=[&](LightCandidate l){ l.blobBbox=addOffsetRect(l.blobBbox); l.rectBbox=addOffsetRect(l.rectBbox); l.center+=cv::Point2f((float)x1,(float)y1); return l; };
        auto addOffsetBody=[&](BodyCandidate b){ b.bbox=addOffsetRect(b.bbox); return b; };

        if (smoothLocal) {
            info.trafficBbox=addOffsetRect(*smoothLocal);
            for (int i=0;i<3;++i) info.slotCenters.emplace_back(x1+smoothLocal->x+smoothLocal->width*(2*i+1)/6.0f,
                                                                    y1+smoothLocal->y+smoothLocal->height/2.0f);
        }
        if (selectedLight) info.selectedLight=addOffsetLight(*selectedLight);
        if (selectedBody) info.selectedBody=addOffsetBody(*selectedBody);
        info.bboxSource=source;
        for (auto l:lights) info.lightCandidates.push_back(addOffsetLight(l));
        for (auto b:bodies) info.bodyCandidates.push_back(addOffsetBody(b));
        for (size_t i=0;i<std::min<size_t>(8,pairs.size());++i) {
            auto p=pairs[i]; p.trafficBbox=addOffsetRect(p.trafficBbox); p.light=addOffsetLight(p.light); p.body=addOffsetBody(p.body); info.pairCandidates.push_back(p);
        }
        return {label,info};
    }
private:
    Args a_;
    BBoxTracker tracker_;
};

// =====================================================================
// [신호등 판단 로직]
//
// Input  : 카메라에서 인식한 신호등/불빛 정보 (DetectionInfo)
// Output : Go(1) or Stop(0)
//
// if 신호등이 있음
//     if 오직 초록색만 켜짐 ([0,0,1])
//         return 1 (GO)
//     else
//         return 0 (STOP, 정지선에서 정지)
// else (신호등이 없음)
//     return 1 (GO, 주행)
// =====================================================================
struct TrafficDecision {
    bool hasTrafficLight = false;        // 신호등 존재 여부 (있으면 true/1, 없으면 false/0)
    std::array<int, 3> lightState{0, 0, 0}; // 색상 상태 배열 [빨, 주, 초] (예: 초록만 켜짐 = [0,0,1])
    int action = 0;                      // 최종 판단: 1 = GO(주행), 0 = STOP(정지선 정지)
};

// 신호등 존재 여부 판단 (1/0)
// - 추적 중인 신호등 bbox가 있거나, 선택된 점등 후보가 있으면 신호등이 있다고 판단
static bool detectTrafficLightExists(const DetectionInfo& info) {
    return info.trafficBbox.has_value() || info.selectedLight.has_value();
}

// 색상 상태 배열 [빨, 주, 초] 생성
// - 프레임 내 모든 점등 후보를 스캔하여 각 색상의 점등 여부를 1/0으로 저장
// - stableLabel(연속 프레임으로 확정된 라벨)도 함께 반영하여 순간 오탐을 보정
static std::array<int, 3> detectLightState(const DetectionInfo& info, char stableLabel) {
    std::array<int, 3> state{0, 0, 0}; // [빨, 주, 초]

    // 빨간불이 켜져있나? (1/0)
    // 주황불이 켜져있나? (1/0)
    // 초록불이 켜져있나? (1/0)
    for (const auto& lc : info.lightCandidates) {
        if (lc.colorLabel == 'R') state[0] = 1;
        if (lc.colorLabel == 'O') state[1] = 1;
        if (lc.colorLabel == 'G') state[2] = 1;
    }

    // 안정화된 라벨 반영 (SafeStableLabel로 확정된 색)
    if (stableLabel == 'R') state[0] = 1;
    if (stableLabel == 'O') state[1] = 1;
    if (stableLabel == 'G') state[2] = 1;

    return state;
}

// 최종 판단 함수: Go(1) or Stop(0)
static TrafficDecision decideAction(const DetectionInfo& info, char stableLabel) {
    TrafficDecision d;

    // 1) 신호등이 있냐 없냐 (1/0)
    d.hasTrafficLight = detectTrafficLightExists(info);

    if (d.hasTrafficLight) {
        // == 신호등 인식 시: 색상 인식 시작 ==
        d.lightState = detectLightState(info, stableLabel);

        const bool red    = (d.lightState[0] == 1); // 빨간불 켜짐 → 정지선에서 stop
        const bool orange = (d.lightState[1] == 1); // 주황불 켜짐 → 정지선에서 stop
        const bool green  = (d.lightState[2] == 1); // 초록불 켜짐 → go (오직 초록만일 때)

        // (0,0,1)의 경우만 주행, 그 외의 경우는 전부 정지선에서 stop
        if (!red && !orange && green) {
            d.action = 1; // GO
        } else {
            d.action = 0; // STOP
        }
    } else {
        // 신호등이 없음 → 주행
        d.action = 1; // GO
    }

    return d;
}

static cv::Scalar colorFor(char c) {
    if (c=='R') return {0,0,255};
    if (c=='O') return {0,165,255};
    if (c=='G') return {0,255,0};
    return {255,255,255};
}

static cv::Mat drawDebug(const cv::Mat& frame, char label, char stable, const DetectionInfo& info, const Args& a, const TrafficDecision& decision) {
    cv::Mat out = frame.clone();
    
    if (a.showSearchRoi) cv::rectangle(out,info.roiRect,{255,255,0},2);
    if (a.showBodyCandidates) for(size_t i=0;i<std::min<size_t>(8,info.bodyCandidates.size());++i) cv::rectangle(out,info.bodyCandidates[i].bbox,{80,80,255},1);
    if (a.showCandidates) for(size_t i=0;i<std::min<size_t>(10,info.lightCandidates.size());++i){cv::rectangle(out,info.lightCandidates[i].blobBbox,{255,255,255},1);cv::rectangle(out,info.lightCandidates[i].rectBbox,{120,120,120},1);}
    if (a.showPairs) for(size_t i=0;i<std::min<size_t>(5,info.pairCandidates.size());++i) cv::rectangle(out,info.pairCandidates[i].trafficBbox,{120,120,120},1);
    if (info.selectedBody) cv::rectangle(out,info.selectedBody->bbox,{255,80,80},2);

    if (info.trafficBbox && a.showTrafficBbox) {
        auto b=*info.trafficBbox;
        cv::rectangle(out,b,{255,0,255},2);
        cv::line(out,{b.x+b.width/3,b.y},{b.x+b.width/3,b.y+b.height},{255,0,255},1);
        cv::line(out,{b.x+2*b.width/3,b.y},{b.x+2*b.width/3,b.y+b.height},{255,0,255},1);
        for(int i=0;i<(int)info.slotCenters.size();++i){char c=slotToLabel(i);cv::circle(out,info.slotCenters[i],4,colorFor(c),cv::FILLED);cv::putText(out,std::string(1,c),info.slotCenters[i]+cv::Point2f(-8,-8),cv::FONT_HERSHEY_SIMPLEX,.5,colorFor(c),1);}
    }

    if (info.selectedLight) {
        auto l=*info.selectedLight; auto col=colorFor(label);
        cv::rectangle(out,l.blobBbox,{255,255,255},2); cv::circle(out,l.center,5,{255,255,255},cv::FILLED);
        cv::rectangle(out,l.rectBbox,col,3); cv::putText(out,std::string(1,label),{l.rectBbox.x,std::max(20,l.rectBbox.y-8)},cv::FONT_HERSHEY_SIMPLEX,.9,col,2);
    
        std::ostringstream dbg;
        dbg << "S=" << (int)l.colorInfo.meanS
        << " G-B=" << (int)l.colorInfo.gb
        << " R-G=" << (int)l.colorInfo.rg;
        cv::putText(out, dbg.str(), {20, 170}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {0,255,255}, 2);
    }

    // ===== 신호등 판단 결과(decision)를 화면에 표시 =====
    const bool red    = decision.lightState[0] == 1;
    const bool orange = decision.lightState[1] == 1;
    const bool green  = decision.lightState[2] == 1;
    const int  colorCount = (red ? 1 : 0) + (orange ? 1 : 0) + (green ? 1 : 0);

    std::string finalAction = (decision.action == 1) ? "GO" : "STOP";
    std::string debugLabel;
    if (!decision.hasTrafficLight)      debugLabel = "NO_TRAFFIC_LIGHT"; // 신호등 없음 → 주행
    else if (colorCount == 1 && green)  debugLabel = "GREEN_ONLY";       // [0,0,1] → 주행
    else if (colorCount == 1 && red)    debugLabel = "RED_ONLY";
    else if (colorCount == 1 && orange) debugLabel = "ORANGE_ONLY";
    else if (colorCount > 1)            debugLabel = "MULTIPLE_COLORS";
    else                                debugLabel = "NO_COLOR";

    std::ostringstream stateStr;
    stateStr << "STATE[R,O,G]: [" << decision.lightState[0] << ","
             << decision.lightState[1] << "," << decision.lightState[2] << "]";

    // ACTION은 STOP=빨강, GO=초록으로 강조. 나머지는 흰색.
    const cv::Scalar white(255, 255, 255);
    const cv::Scalar actionColor = (decision.action == 1) ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255);
    cv::putText(out, "ACTION: " + finalAction + " (" + std::to_string(decision.action) + ")", {20, 50}, cv::FONT_HERSHEY_SIMPLEX, 1.0, actionColor, 2);
    cv::putText(out, "LIGHT EXIST: " + std::string(decision.hasTrafficLight ? "1" : "0"), {20, 90}, cv::FONT_HERSHEY_SIMPLEX, 0.8, white, 2);
    cv::putText(out, stateStr.str(), {20, 130}, cv::FONT_HERSHEY_SIMPLEX, 0.7, white, 2);
    cv::putText(out, "DEBUG: " + debugLabel, {20, 205}, cv::FONT_HERSHEY_SIMPLEX, 0.7, white, 2);
    return out;
}

class FrameSaver {
public:
    explicit FrameSaver(const Args& a):a_(a){
        sessionDir_=fs::path(expandUserPath(a.saveDir))/("session_"+nowString("%Y%m%d_%H%M%S"));
        rawDir_=sessionDir_/"raw"; debugDir_=sessionDir_/"debug";
        fs::create_directories(rawDir_); if(a.saveDebug) fs::create_directories(debugDir_);
        std::cout<<"[INFO] frame save directory: "<<sessionDir_<<"\n";
    }
    void save(const cv::Mat& raw,const cv::Mat& debug,char label,char stable){
        frameIndex_++;
        if((frameIndex_-1)%a_.saveEveryN!=0) return;
        if(a_.saveOnlyDetected && label!='R' && label!='O' && label!='G') return;
        std::ostringstream fn;fn<<"frame_"<<std::setw(8)<<std::setfill('0')<<frameIndex_<<"_raw-"<<label<<"_stable-"<<stable<<"_"<<nowStringWithMs()<<"."<<a_.saveFormat;
        std::vector<int> params;
        if(a_.saveFormat=="jpg") params={cv::IMWRITE_JPEG_QUALITY,a_.jpegQuality}; else params={cv::IMWRITE_PNG_COMPRESSION,3};
        if(!cv::imwrite((rawDir_/fn.str()).string(),raw,params)){std::cerr<<"[WARN] raw frame save failed\n";return;}
        if(a_.saveDebug && !cv::imwrite((debugDir_/fn.str()).string(),debug,params)) std::cerr<<"[WARN] debug frame save failed\n";
        savedCount_++;
    }
    long frameIndex()const{return frameIndex_;} long savedCount()const{return savedCount_;} const fs::path& sessionDir()const{return sessionDir_;}
private: Args a_; fs::path sessionDir_,rawDir_,debugDir_; long frameIndex_=0,savedCount_=0;
};

static bool openOne(cv::VideoCapture& cap,const std::string& camera,const Args& a){
    bool ok=isNumber(camera)?cap.open(std::stoi(camera),cv::CAP_V4L2):cap.open(camera,cv::CAP_V4L2);
    if(!ok){cap.release();ok=isNumber(camera)?cap.open(std::stoi(camera),cv::CAP_ANY):cap.open(camera,cv::CAP_ANY);}
    if(!ok)return false;
    cap.set(cv::CAP_PROP_FRAME_WIDTH,a.width);cap.set(cv::CAP_PROP_FRAME_HEIGHT,a.height);
    if(a.hasManualExposure){cap.set(cv::CAP_PROP_AUTO_EXPOSURE,1);cap.set(cv::CAP_PROP_EXPOSURE,a.manualExposure);}
    cv::Mat f;if(!cap.read(f)||f.empty()){cap.release();return false;}return true;
}

int main(int argc,char** argv){
    try{
        Args a=parseArgs(argc,argv);
        cv::VideoCapture cap;
        if(!openOne(cap,a.camera,a)){
            bool opened=false;
            if(isNumber(a.camera))for(int i=0;i<6&&!opened;++i)if(std::to_string(i)!=a.camera&&openOne(cap,std::to_string(i),a)){std::cerr<<"[WARN] camera "<<a.camera<<" failed; fallback "<<i<<" opened\n";opened=true;}
            if(!opened)throw std::runtime_error("camera open failed");
        }else std::cout<<"[INFO] camera opened: "<<a.camera<<"\n";

        TrafficLightDetector detector(a); SafeStableLabel stabilizer(a.confirm); FrameSaver saver(a);
        char last='\0'; int lastAction=-1;
        for(;;){
            cv::Mat frame;if(!cap.read(frame)||frame.empty()){std::cerr<<"camera read failed\n";break;}
            frame=rotateFrame(frame,a.rotate);if(!a.allowPortrait)frame=ensureLandscape(frame);
            auto [label,info]=detector.detect(frame);char stable=stabilizer.update(label);

            // ===== 신호등 판단: Go(1) or Stop(0) =====
            TrafficDecision decision = decideAction(info, stable);

            // 상태가 바뀔 때(또는 --print-every-frame)마다 출력
            // 출력 형식: EXIST=<1/0> STATE=[빨,주,초] ACTION=<1/0>
            if(a.printEveryFrame||stable!=last||decision.action!=lastAction){
                std::cout<<"EXIST="<<(decision.hasTrafficLight?1:0)
                         <<" STATE=["<<decision.lightState[0]<<","<<decision.lightState[1]<<","<<decision.lightState[2]<<"]"
                         <<" ACTION="<<decision.action<<(decision.action==1?" (GO)":" (STOP)")
                         <<" LABEL="<<stable<<std::endl;
                last=stable;lastAction=decision.action;
            }
            cv::Mat debug=drawDebug(frame,label,stable,info,a,decision);saver.save(frame,debug,label,stable);
            cv::imshow("traffic_light_test",debug);
            if(a.showMask){cv::imshow("white_mask",info.whiteMask);cv::imshow("active_mask",info.activeMask);cv::imshow("dark_body_mask",info.darkMask);}
            if((cv::waitKey(1)&0xFF)=='q')break;
        }
        cap.release();cv::destroyAllWindows();
        std::cout<<"[INFO] total captured frames: "<<saver.frameIndex()<<"\n";
        std::cout<<"[INFO] total saved images: "<<saver.savedCount()<<"\n";
        std::cout<<"[INFO] saved at: "<<saver.sessionDir()<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"[ERROR] "<<e.what()<<"\n";return 1;}
}