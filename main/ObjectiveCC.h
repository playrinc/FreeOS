//
//  ObjectiveCC.h
//  Objective CC
//
//  Created by Chris Galzerano on 1/22/25.
//

#ifndef ObjectiveCC_h
#define ObjectiveCC_h

#include <stdio.h>
#include <stdbool.h>
#include <CPUGraphics.h>
#include <time.h>
#include <sys/time.h>
#include <LogWrapper.h>

#if defined(__aarch64__) || defined(__x86_64__)
typedef unsigned long UInteger;
#else
typedef unsigned int UInteger;
#endif

#if defined(__aarch64__) || defined(__x86_64__)
typedef long Integer;
#else
typedef int Integer;
#endif

#if defined(__aarch64__) || defined(__x86_64__)
typedef double Float;
#else
typedef float Float;
#endif

typedef enum {
    CCType_Range,
    CCType_Point,
    CCType_Size,
    CCType_Rect,
    CCType_StringEncoding,
    CCType_String,
    CCType_Number,
    CCType_Date,
    CCType_DateFormatter,
    CCType_Calendar,
    CCType_DateComponents,
    CCType_TimeZone,
    CCType_Locale,
    CCType_Data,
    CCType_Archiver,
    CCType_Array,
    CCType_KeyValuePair,
    CCType_Dictionary,
    CCType_SortDescriptor,
    CCType_RegularExpression,
    CCType_JSONObject,
    CCType_Null,
    CCType_Thread,
    CCType_URLResponse,
    CCType_URLRequest,
    CCType_SerialPort,
    CCType_Color,
    CCType_Gradient,
    CCType_Layer,
    CCType_View,
    CCType_FramebufferView,
    CCType_Font,
    CCType_Label,
    CCType_PointPath,
    CCType_ShapeLayer,
    CCType_Image,
    CCType_ImageView,
    CCType_GraphicsContext,
    CCType_Transform,
    CCType_Transform3D,
    CCType_GestureRecognizer,
    CCType_ScrollView,
    CCType_TextView
} CCType;

typedef struct {
    CCType type;
    UInteger loc;
    UInteger len;
} CCRange;

typedef struct {
    CCType type;
    float x;
    float y;
} CCPoint;

typedef struct {
    CCType type;
    float width;
    float height;
} CCSize;

typedef struct {
    CCType type;
    CCPoint* origin;
    CCSize* size;
} CCRect;

typedef enum {
    UTF8StringEncoding,
    // Future encodings can be added here
} StringEncoding;

typedef enum {
    CCShapeTypePoints,
    CCShapeTypeLines,
    CCShapeTypeLineLoop,
    CCShapeTypeLineStrip,
    CCShapeTypePolygon,
    CCShapeTypeTriangles,
    CCShapeTypeTriangleFan,
    CCShapeTypeTriangleStrip,
} CCShapeType;

CCRange* ccRange(UInteger location, UInteger length);
CCPoint* ccPoint(float x, float y);
CCSize* ccSize(float width, float height);
CCRect* ccRect(float x, float y, float width, float height);

void freeCCRect(CCRect* rect);

typedef struct {
    CCType type;
    UInteger length;
    char* string;
} CCString;

typedef struct {
    CCType type;
    double doubleValue;
} CCNumber;

typedef struct {
    CCType type;
    double timeValue;
} CCDate;

typedef enum {
    CCCalendarUnitEra,
    CCCalendarUnitYear,
    CCCalendarUnitMonth,
    CCCalendarUnitDay,
    CCCalendarUnitHour,
    CCCalendarUnitMinute,
    CCCalendarUnitSecond,
    CCCalendarUnitWeekday,
    CCCalendarUnitWeekdayOrdinal,
    CCCalendarUnitQuarter,
    CCCalendarUnitWeekOfMonth,
    CCCalendarUnitWeekOfYear,
    CCCalendarUnitYearForWeekOfYear,
    CCCalendarUnitNanosecond,
    CCCalendarUnitCalendar,
    CCCalendarUnitTimeZone
} CCCalendarUnit;

typedef enum {
    CCCalendarIdentifierGregorian,
    CCCalendarIdentifierBuddhist,
    CCCalendarIdentifierChinese,
    CCCalendarIdentifierCoptic,
    CCCalendarIdentifierHebrew,
    CCCalendarIdentifierISO8601,
    CCCalendarIdentifierIndian,
    CCCalendarIdentifierIslamic,
    CCCalendarIdentifierJapanese,
    CCCalendarIdentifierPersian,
    CCCalendarIdentifierRepublicOfChina,
    CCCalendarIdentifierIslamicTabular
} CCCalendarIdentifier;

typedef struct {
    CCType type;
    double secondsFromGmt;
    CCString* name;
    CCString* abbreviation;
} CCTimeZone;

typedef struct {
    CCType type;
    CCString* localeIdentifier;
    CCString* languageCode;
    CCString* countryCode;
    CCString* scriptCode;
    CCString* variantCode;
    //NSCharacterSet *exemplarCharacterSet;
    CCString* calendarIdentifier;
    CCString* collationIdentifier;
    bool usesMetricSystem;
    CCString* decimalSeparator;
    CCString* groupingSeparator;
    CCString* currencySymbol;
    CCString* currencyCode;
    CCString* collatorIdentifier;
    CCString* quotationBeginDelimiter;
    CCString* quotationEndDelimiter;
    CCString* alternateQuotationBeginDelimiter;
    CCString* alternateQuotationEndDelimiter;
} CCLocale;

typedef struct {
    CCType type;
    CCCalendarIdentifier identifier;
    CCTimeZone* timeZone;
    CCLocale* locale;
} CCCalendar;

typedef struct {
    CCType type;
    CCCalendar *calendar;
    CCTimeZone *timeZone;
    Integer era;
    Integer year;
    Integer month;
    Integer day;
    Integer hour;
    Integer minute;
    Integer second;
    Integer nanosecond;
    Integer weekday;
    Integer weekdayOrdinal;
    Integer quarter;
    Integer weekOfMonth;
    Integer weekOfYear;
    Integer yearForWeekOfYear;
    bool leapMonth;
    CCDate *date;
} CCDateComponents;

typedef enum {
    CCDateFormatterStyleShort,
    CCDateFormatterStyleMedium,
    CCDateFormatterStyleLong,
} CCDateFormatterStyle;

typedef struct {
    CCType type;
    CCDateFormatterStyle dateStyle;
    CCDateFormatterStyle timeStyle;
    CCString* dateFormat;
    CCLocale* locale;
    CCTimeZone* timeZone;
    CCCalendar* calendar;
} CCDateFormatter;

typedef struct {
    CCType type;
    void *bytes;
    UInteger length;
} CCData;



typedef struct {
    CCType type;
} CCNull;

typedef enum {
    CCJSONWriteStyleReadable,
    CCJSONWriteStyleCompressed
} CCJSONWriteStyle;

typedef struct {
    CCType type;
    CCString* jsonString;
    void* jsonObject;
} CCJSONObject;

typedef struct {
    CCType type;
    int count;
    void** array;
} CCArray;

typedef struct {
    CCType type;
    CCString *key;
    void* value;
} CCKeyValuePair;

typedef struct {
    CCType type;
    CCKeyValuePair* items;
    Integer count;
} CCDictionary;

typedef struct {
    CCType type;
    CCString* key;
    bool ascending;
} CCSortDescriptor;

typedef enum {
   CCRegularExpressionCaseInsensitive             = 1 << 0,     /* Match letters in the pattern independent of case. */
   CCRegularExpressionAllowCommentsAndWhitespace  = 1 << 1,     /* Ignore whitespace and #-prefixed comments in the pattern. */
   CCRegularExpressionIgnoreMetacharacters        = 1 << 2,     /* Treat the entire pattern as a literal string. */
   CCRegularExpressionDotMatchesLineSeparators    = 1 << 3,     /* Allow . to match any character, including line separators. */
   CCRegularExpressionAnchorsMatchLines           = 1 << 4,     /* Allow ^ and $ to match the start and end of lines. */
   CCRegularExpressionUseUnixLineSeparators       = 1 << 5,     /* Treat only \n as a line separator (otherwise, all standard line separators are used). */
   CCRegularExpressionUseUnicodeWordBoundaries    = 1 << 6      /* Use Unicode TR#29 to specify word boundaries (otherwise, traditional regular expression word boundaries are used). */
} CCRegularExpressionOptions;

typedef struct {
    CCType type;
    CCString* pattern;
    CCRegularExpressionOptions options;
} CCRegularExpression;

typedef struct {
    CCType type;
    Float r;
    Float g;
    Float b;
    Float a;
} CCColor;

// High-level Gradient Structure
typedef struct {
    CCType type;
    CCArray* colors;     // Array of CCColor* objects
    CCArray* locations;  // Array of CCNumber* objects (0.0 to 1.0)
    Float angle;         // Angle in radians
    GradientType gradType;
} CCGradient;

typedef struct {
    CCType type;
    CCSize* scale;
    CCPoint* translation;
    Float rotationAngle;
    float* matrix;
} CCTransform;

typedef struct {
    float m11, m12, m13, m14;
    float m21, m22, m23, m24;
    float m31, m32, m33, m34;
    float m41, m42, m43, m44;
} CCTransform3DMatrix;

typedef struct {
    CCType type;
    CCTransform3DMatrix matrix;
} CCTransform3D;

typedef struct {
    CCType type;
    CCArray* points;
} CCPointPath;

typedef struct {
    CCType type;
    CCPointPath* pointPath;
    CCColor* fillColor;
    CCShapeType shapeType;
    CCGradient* gradient;
} CCShapeLayer;

typedef struct {
    CCType type;
    CCRect* frame;
    CCColor* backgroundColor;
    bool masksToBounds;
    Float cornerRadius;
    CCColor* borderColor;
    Float borderWidth;
    CCPoint* shadowOffset;
    Float shadowRadius;
    Float shadowOpacity;
    CCColor* shadowColor;
    struct CCLayer* superlayer;
    CCArray* sublayers;
    CCTransform* transform;
    CCTransform3D* transform3D;
    CCGradient* gradient;
} CCLayer;

typedef struct {
    CCType type;
    CCRect* frame;
    CCColor* backgroundColor;
    CCLayer* layer;
    CCShapeLayer* shapeLayer;
    struct CCView* superview;
    CCArray* subviews;
    CCTransform* transform;
    CCArray* gestureRecognizers;
    int tag;
    bool ignoreTouch;
} CCView;

typedef enum {
    CCLineBreakWordWrapping,
    CCLineBreakHyphenation,
    CCLineBreakEllipsis
} CCLineBreakMode;

typedef enum {
    CCTextAlignmentLeft,
    CCTextAlignmentCenter,
    CCTextAlignmentRight
} CCTextAlignmentMode;

typedef enum {
    CCTextVerticalAlignmentTop,
    CCTextVerticalAlignmentCenter,
    CCTextVerticalAlignmentBottom
} CCTextVerticalAlignmentMode;

typedef struct {
    CCType type;
    CCString* name;
    CCString* filePath;
    Float renderingSize;
    bool isLoaded;
} CCFont;

typedef struct {
    CCType type;
    CCView* view;
    CCString* text;
    CCColor* textColor;
    Float fontSize;
    CCFont* font;
    CCString* fontName;
    CCTextAlignmentMode textAlignment;
    CCTextVerticalAlignmentMode textVerticalAlignment;
    CCLineBreakMode lineBreakMode;
    Float lineSpacing;   // Pixels between lines
    Float glyphSpacing;  // Pixels between characters
    int tag;
    bool ignoreTouch;
} CCLabel;

typedef struct {
    CCType type;
    CCString* filePath;
    unsigned char* imageData;
    CCSize* size;
    unsigned int texture;
    bool ignoreTouch;
} CCImage;

typedef struct {
    CCType type;
    CCSize* size;
    unsigned int fbo;
    unsigned int texture;
} CCGraphicsContext;

typedef struct {
    CCType type;
    CCView* view;
    Framebuffer* framebuffer;
    int tag;
    bool ignoreTouch;
} CCFramebufferView;

typedef struct {
    CCType type;
    CCView* view;
    CCImage* image;
    int tag;
    bool ignoreTouch;
    float alpha;
} CCImageView;

// 2. CCScrollView: A generic container that clips and scrolls its content
typedef struct {
    CCType type;
    CCView* view;           // The "Window" (Viewport) - masksToBounds = true
    CCView* contentView;    // The "Sheet" (Holds subviews) - moves up/down
    CCPoint* contentOffset; // Current scroll position (x, y)
    CCSize* contentSize;    // Total size of the scrollable area
    int tag;
} CCScrollView;

// 3. CCTextView: A specialized wrapper around ScrollView + Label
typedef struct {
    CCType type;
    CCScrollView* scrollView; // Handles the scrolling mechanics
    CCLabel* label;           // Handles the text rendering
    FTC_Manager    ftManager;
    FTC_ImageCache ftImageCache;
    FTC_CMapCache  ftCMapCache;
    int tag;
} CCTextView;

typedef void (*CCGestureAction)(void*);

typedef enum {
    CCGestureMouse,
    CCGestureTouch,
    CCGestureUniversal
} CCGestureRecognizerType;

typedef enum {
    CCGestureUp,
    CCGestureDown,
    CCGesturePositionChanged
} CCGestureType;

typedef struct {
    CCType type;
    CCGestureRecognizerType recognizerType;
    CCGestureAction action;
    void* view;
    CCPoint* position;
    CCGestureType gestureType;
} CCGestureRecognizer;

typedef struct {
    CCType type;
    CCData* data;
    CCString* contentType;
} CCURLResponse;

typedef enum {
    CCURLRequestGET,
    CCURLRequestPOST
} CCURLRequestType;

typedef struct {
    CCType type;
    CCURLRequestType requestType;
    CCString* url;
    CCData* data;
    CCArray* headers;
} CCURLRequest;

typedef void (*CCSerialPortCallback)(const char *data);

typedef struct {
    CCType type;
    CCString* name;
    Integer baudRate;
    int fd;
    CCSerialPortCallback callback;
    bool listening;
} CCSerialPort;

typedef void* (*CCThreadFunction)(void*);
typedef void (*CCThreadCompletionCallback)(void*);

typedef enum {
    CCThreadMain,
    CCThreadBackground
} CCThreadType;

typedef struct {
    CCType type;
    CCThreadType threadType;
    CCThreadFunction function;
    CCThreadCompletionCallback callback;
    void* functionArg;
    void* callbackArg;
} CCThread;

//Threading Functions
CCThread* ccThread(void);
CCThread* ccThreadWithType(CCThreadType threadType);
CCThread* ccThreadWithParameters(CCThreadType threadType, CCThreadFunction function, CCThreadCompletionCallback callback, void* functionArg, void* callbackArg);
void* threadWrapper(void* arg);
void threadExecute(CCThread* thread);

//URL Request Functions
CCURLRequest* urlRequest(void);
CCURLRequest* urlRequestWithUrl(CCString* url);
CCString* urlRequestHeader(CCString* key, CCString* value);
CCURLResponse* urlResponse(void);
CCURLResponse* urlRequestExecute(CCURLRequest* urlRequest, char** contentType);

//Serial Port Functions
CCSerialPort* serialPort(void);
CCSerialPort* serialPortWithName(CCString* name);
CCArray* serialPortsList(void);
int serialPortOpen(CCSerialPort* serialPort);
void serialPortClose(CCSerialPort* serialPort);
void serialPortAddListener(CCSerialPort* serialPort);
void serialPortRemoveListener(CCSerialPort* serialPort);
void serialPortSendData(CCSerialPort* serialPort, CCString* data);

//Log Functions
void ccLog(const char* format, ...);
void ccLogString(CCString* string);
CCString* objectDescription(void* object);
CCString* stringForType(CCType type);

//Graphics Functions
CCColor* color(Float r, Float b, Float g, Float a);
CCGradient* gradientWithColors(CCArray* colors, CCArray* locations, Float angle);
ColorRGBA convert_cc_color(CCColor* cc);
CCColor* convert_colorrgba_to_cccolor(ColorRGBA c);
Gradient* create_low_level_gradient(CCGradient* ccGrad);
CCFont* font(CCString* filePath, Float renderingSize);
CCLayer* layer(void);
CCLayer* layerWithFrame(CCRect* frame);
void layerSetCornerRadius(CCLayer* layer, Float cornerRadius);

CCView* view(void);
CCView* viewWithFrame(CCRect* frame);
void viewAddSubview(void* view, void* subview);
void viewRemoveFromSuperview(void* view);
void viewSetBackgroundColor(void* view, CCColor* color);
void viewSetFrame(void* view, CCRect* frame);
void viewAddGestureRecognizer(void* view, CCGestureRecognizer* gestureRecognizer);
void freeViewHierarchy(CCView* view);
void freeCCGradient(CCGradient* gradient);

CCGestureRecognizer* gestureRecognizerWithType(CCGestureType gestureType, CCGestureAction action);

CCFramebufferView* framebufferView(void);
CCFramebufferView* framebufferViewWithFrame(CCRect* frame);
void framebufferViewSetFramebuffer(CCFramebufferView* fbView, Framebuffer* fb);

CCLabel* label(void);
CCLabel* labelWithFrame(CCRect* frame);
void labelSetText(CCLabel* label, CCString* text);

CCPointPath* pointPath(void);
CCPointPath* pointPathWithPoints(CCArray* points);
void pointPathAddPoint(CCPointPath* pointPath, CCPoint* point);
CCShapeLayer* shapeLayer(void);
CCShapeLayer* shapeLayerWithPointPath(CCPointPath* pointPath);

CCImage* image(void);
CCImage* imageWithFile(CCString* filePath);
CCImage* imageWithData(unsigned char* imageData, Integer width, Integer height);
CCImageView* imageView(void);
CCImageView* imageViewWithFrame(CCRect* frame);
void imageViewSetImage(CCImageView* imageView, CCImage* image);

CCScrollView* scrollViewWithFrame(CCRect* frame);
void scrollViewSetContentSize(CCScrollView* sv, CCSize* size);
void scrollViewSetContentOffset(CCScrollView* sv, CCPoint* offset);

CCTextView* textViewWithFrame(CCRect* frame);
void textViewSetText(CCTextView* tv, CCString* text); // Auto-calculates height
void freeCCScrollView(CCScrollView* sv);

CCGraphicsContext* graphicsContextCreate(Float width, Float height);

bool rectContainsPoint(CCRect* rect, CCPoint* point);
bool rectContainsRect(CCRect* rect, CCRect* rect1);

//Drawing transformations
CCTransform* transform(void);
CCTransform* transformWithMatrix(float* matrix);
CCTransform* transformRotate(Float rotationAngle);
CCTransform* transformTranslation(Float x, Float y);
CCTransform* transformScale(Float x, Float y);
CCTransform* transformConcat(CCTransform* transform1, CCTransform* transform2);
bool transformEqualsTransform(CCTransform* transform1, CCTransform* transform2);
float* CCTransformScaleMatrix(Float x, Float y);
float* CCTransformRotateMatrix(Float rotationAngle);
float* CCTransformTranslateMatrix(Float x, Float y);
float* CCTransformConcatMatrix(float* matrix, float* matrix1);
bool transformEqualsTransformMatrix(float* transform1, float* transform2);
CCTransform3D* transform3D(void);
CCTransform3D* transform3DWithMatrix(CCTransform3DMatrix matrix);
CCTransform3DMatrix CCTransform3DIdentity(void);
CCTransform3DMatrix CCTransform3DMakeScale(float sx, float sy, float sz);
CCTransform3DMatrix CCTransform3DMakeRotation(float angle, float x, float y, float z);
CCTransform3DMatrix CCTransform3DMakeTranslation(float tx, float ty, float tz);
CCTransform3DMatrix CATransform3DMultiply(CCTransform3DMatrix a, CCTransform3DMatrix b);
bool transform3DEqualsTransform3D(CCTransform3DMatrix* transform, CCTransform3DMatrix* transform1);

float afb(CCView* view);
float afr(CCView* view);

//Program Main Function
int mainProgram(void);

//Program Utility Functions
CCString* resourceFilePath(CCString* filePath);

//String Functions
CCString* string(void);
CCString* ccs(const char * string);
CCString* stringWithCString(const char * string);
CCString* stringWithFormat(const char* format, ...);
CCString* substringWithRange(CCString* string, CCRange range);
CCString* substringFromIndex(CCString* string, int index);
CCString* substringToIndex(CCString* string, int index);
CCString* replaceOccurencesOfStringWithString(CCString* string, CCString* target, CCString* replacement);
CCString* stringByAppendingString(CCString* string, CCString* string1);
CCString* stringByAppendingFormat(CCString *string, const char* format, ...);
CCArray* stringPathComponents(CCString* string);
CCString* stringLastPathComponent(CCString* string);
CCString* stringFileExtension(CCString* string);
bool stringContainsString(CCString* string, CCString* containsString);
bool stringEqualsString(CCString* string, CCString* equalsString);
CCString* stringLowercase(CCString* string);
CCString* stringUppercase(CCString* string);
CCString* stringCapitalized(CCString* string);
void appendString(CCString* string, CCString* stringToAppend);
void appendFormat(CCString* string, const char* format, ...);
CCArray* stringComponentsSeparatedByString(CCString* string, CCString* separator);
CCString* stringsCombinedWithString(CCArray* strings, CCString* combiner);
CCData* stringDataWithEncoding(CCString* string, StringEncoding encoding);
CCString* stringFromDataWithEncoding(CCData* data, StringEncoding encoding);
CCString* stringWithContentsOfFile(CCString* filePath);
const char* cStringOfString(CCString* string);
int stringIntValue(CCString* string);
float stringFloatValue(CCString* string);
long stringLongValue(CCString* string);
double stringDoubleValue(CCString* string);
CCString* copyCCString(const CCString* original);
void freeCCString(CCString* ccstring);

//Number Functions
CCNumber* numberWithInt(int number);
CCNumber* numberWithFloat(float number);
CCNumber* numberWithLong(long number);
CCNumber* numberWithDouble(double number);
int numberIntValue(CCNumber* number);
float numberFloatValue(CCNumber* number);
long numberLongValue(CCNumber* number);
double numberDoubleValue(CCNumber* number);
CCNumber* copyCCNumber(const CCNumber* original);
void freeCCNumber(CCNumber* number);

// Macro to automatically append NULL
#define CCArrayWithObjects(...) arrayWithObjects(__VA_ARGS__, NULL)

//Array Functions
CCArray* array(void);
CCArray* arrayWithArray(CCArray* array);
CCArray* arrayWithObjects(void* object, ...);
void arrayAddObject(CCArray* array, void* object);
void arrayAddObjectsFromArray(CCArray* array, CCArray *array2);
void arrayInsertObjectAtIndex(CCArray* array, void* object, UInteger index);
void* arrayObjectAtIndex(CCArray* array, UInteger index);
void arrayDeleteObjectAtIndex(CCArray* array, UInteger index);
int arrayIndexOfObject(CCArray* ccArray, void* object);
void arrayRemoveObject(CCArray* ccArray, void* object);
UInteger arrayCount(CCArray* array);
void* copyElement(void* element);
void freeElement(void* element);
CCArray* copyCCArray(const CCArray* original);
void freeCCArray(CCArray* array);

//Dictionary Functions
CCDictionary* dictionary(void);
CCDictionary* dictionaryWithDictionary(CCDictionary* dictionary);
CCDictionary* dictionaryWithKeysAndObjects(CCString* key, ...);
void dictionarySetObjectForKey(CCDictionary* dictionary, void* object, CCString* key);
void* dictionaryObjectForKey(CCDictionary* dictionary, CCString* key);
void* dictionaryObjectForKeyFreeKey(CCDictionary* dictionary, CCString* key);
CCArray* dictionaryAllKeys(CCDictionary* dictionary);
CCArray* dictionaryAllObjects(CCDictionary* dictionary);
CCDictionary* copyCCDictionary(const CCDictionary* original);
void freeCCDictionary(CCDictionary* dict);

//Sort Descriptor Functions
CCSortDescriptor* sortDescriptor(void);
CCSortDescriptor* sortDescriptorWithKey(CCString* key, bool ascending);
CCArray* sortedArrayUsingSortDescriptor(CCArray* array, CCSortDescriptor *sortDescriptor);
CCSortDescriptor* copyCCSortDescriptor(const CCSortDescriptor* original);
void freeCCSortDescriptor(CCSortDescriptor* descriptor);

//Internal Sort Descriptor Functions
void swap(void** array, int i, int j);
int partition(CCArray* array, int low, int high, CCSortDescriptor* sortDescriptor);
void quickSort(CCArray* array, int low, int high, CCSortDescriptor* sortDescriptor);
int compareDictionaryValues(void* value1, void* value2, bool ascending);
int compareNumberValues(double num1, double num2, bool ascending);

//Regular Expression Functions
CCRegularExpression* regularExpression(void);
CCRegularExpression* regularExpressionWithPattern(CCString* pattern, CCRegularExpressionOptions options);
CCArray* matchesInString(CCRegularExpression* regex, CCString* string, CCRange* range);

//Data Functions
CCData* data(void);
CCData* dataWithData(CCData* data);
CCData* dataWithBytes(void* bytes, UInteger length);
bool dataIsEqualToData(CCData* data, CCData *data1);
CCData* dataWithContentsOfFile(CCString* filePath);
bool dataWriteToFile(CCData* data, CCString* path);
//void* dataGetBytes(void* buffer, UInteger length);
//void* dataGetBytes(void* buffer, CCRange range);
CCData* copyCCData(const CCData* original);
void freeCCData(CCData* data);

//JSON Functions
CCJSONObject* jsonObject(void);
CCJSONObject* jsonObjectWithJSONString(CCString* string);
CCJSONObject* jsonObjectWithObject(void* object);
void generateJsonStringFromObject(CCJSONObject* object, CCJSONWriteStyle writeStyle);
void generateObjectFromJsonString(CCJSONObject* object);

//Internal cJSON Functions
CCDictionary* dictionaryForJsonObject(void* jsonObject1);
CCArray* arrayForJsonObject(void* jsonObject1);
void* cJsonArrayForArray(CCArray* array);
void* cJsonDictionaryForDictionary(CCDictionary* dictionary);

//Null Functions
CCNull* null(void);
void* cc_safe_alloc(size_t count, size_t size);
void* cc_safe_realloc(void* ptr, size_t new_size);
// Macro to safely free and NULL the pointer
#define CC_SAFE_FREE(ptr) do { if (ptr) { free(ptr); (ptr) = NULL; } } while(0)

//Date Functions
CCDate* date(void);
CCDate* dateWithTimeInterval(double timeInterval);
void dateAddTimeInterval(CCDate* date, double timeInterval);
bool dateEarlierThanDate(CCDate* date, CCDate* date1);
bool dateLaterThanDate(CCDate* date, CCDate* date1);
bool dateEqualToDate(CCDate* date, CCDate* date1);
CCDate* copyCCDate(const CCDate* original);
void freeCCDate(CCDate* date);

//Date Formatter Functions
CCDateFormatter* dateFormatter(void);
CCDate* dateFromString(CCDateFormatter* dateFormatter, CCString* string);
CCString* stringFromDate(CCDateFormatter* dateFormatter, CCDate* date);

//Calendar Functions
CCCalendar* calendar(void);
CCCalendar* calendarWithIdentifier(CCCalendarIdentifier identifier);
CCDateComponents* componentsFromDate(CCDate* date);
CCDate* dateFromComponents(CCDateComponents* components);
Integer dateComponentValueForDate(CCDate* date, CCCalendarUnit calendarUnit);

//Date Components Functions
void dateComponentsSetValueForComponent(CCDateComponents* dateComponents, Integer value, CCCalendarUnit component);
Integer dateComponentsValueForComponent(CCDateComponents* dateComponents, CCCalendarUnit component);

//Time Zone Functions
CCTimeZone* timeZone(void);
CCTimeZone* timeZoneWithName(CCString* name);
CCTimeZone* systemTimeZone(void);
CCArray* timeZoneNames(void);

//Locale Functions
CCLocale* locale(void);
CCLocale* localeWithIdentifier(CCString* identifier);
CCArray* localeIdentifiers(void);

#endif /* ObjectiveCC_h */
