#include <obs-module.h>
#include <obs-frontend-api.h>
#include <graphics/graphics.h>
#include <plugin-support.h>

#include <QBoxLayout>
#include <QFile>
#include <QFormLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "ja-JP")

namespace {
constexpr uint32_t kCanvasWidth = 1280;
constexpr uint32_t kCanvasHeight = 720;
struct MapEntry { QString name; QString code; bool completed = false; };
QMutex g_imageMutex;
QImage g_overlayImage;
uint64_t g_imageRevision = 0;
std::vector<MapEntry> g_maps;

QString configPath()
{
	char *path = obs_module_config_path("maps.json");
	QString result = QString::fromUtf8(path ? path : "");
	bfree(path);
	return result;
}

void saveMaps()
{
	QJsonArray array;
	for (const auto &map : g_maps) {
		QJsonObject object;
		object["name"] = map.name;
		object["code"] = map.code;
		object["completed"] = map.completed;
		array.append(object);
	}
	QFile file(configPath());
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
}

void loadMaps()
{
	QFile file(configPath());
	if (!file.open(QIODevice::ReadOnly)) return;
	const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
	if (!document.isArray()) return;
	for (const QJsonValue &value : document.array()) {
		const QJsonObject object = value.toObject();
		MapEntry entry{object["name"].toString(), object["code"].toString(), object["completed"].toBool()};
		if (!entry.name.trimmed().isEmpty()) g_maps.push_back(entry);
	}
}

void roundedRect(QPainter &painter, const QRectF &rect, qreal radius, const QColor &color)
{
	painter.setPen(Qt::NoPen);
	painter.setBrush(color);
	painter.drawRoundedRect(rect, radius, radius);
}

void drawText(QPainter &painter, const QRectF &rect, const QString &text, int size, int weight,
	      const QColor &color, Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter)
{
	QFont font("Arial");
	font.setPixelSize(size);
	font.setWeight(static_cast<QFont::Weight>(weight));
	painter.setFont(font);
	painter.setPen(color);
	painter.drawText(rect, alignment, text);
}

QImage renderOverlay()
{
	QImage image(kCanvasWidth, kCanvasHeight, QImage::Format_RGBA8888);
	image.fill(Qt::transparent);
	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setRenderHint(QPainter::TextAntialiasing, true);
	const QColor green(46, 235, 118), panel(9, 16, 25, 235), subPanel(21, 31, 43, 242);
	const QColor white(245, 249, 252), muted(154, 169, 185);
	roundedRect(painter, QRectF(26, 26, 1228, 668), 28, panel);
	roundedRect(painter, QRectF(26, 26, 12, 668), 6, green);
	drawText(painter, QRectF(70, 48, 1140, 48), "FORTNITE CREATIVE  •  MAP ROTATION", 27, QFont::Bold, green);
	int current = -1, next = -1, completed = 0;
	for (int i = 0; i < static_cast<int>(g_maps.size()); ++i) {
		if (g_maps[i].completed) ++completed;
		else if (current < 0) current = i;
		else if (next < 0) next = i;
	}
	roundedRect(painter, QRectF(70, 120, 710, 214), 22, subPanel);
	drawText(painter, QRectF(100, 140, 650, 34), current < 0 ? "ALL CLEAR" : "NOW PLAYING", 20,
		 QFont::Bold, current < 0 ? green : muted);
	drawText(painter, QRectF(100, 176, 650, 76), current < 0 ? QStringLiteral("全マップ完了！") : g_maps[current].name,
		 47, QFont::Black, white);
	if (current >= 0) drawText(painter, QRectF(100, 260, 650, 44), g_maps[current].code, 29, QFont::DemiBold, green);
	roundedRect(painter, QRectF(810, 120, 400, 214), 22, subPanel);
	drawText(painter, QRectF(840, 140, 340, 34), "NEXT MAP", 20, QFont::Bold, muted);
	drawText(painter, QRectF(840, 182, 340, 62), next < 0 ? QStringLiteral("—") : g_maps[next].name, 31, QFont::Bold, white);
	if (next >= 0) drawText(painter, QRectF(840, 252, 340, 40), g_maps[next].code, 23, QFont::DemiBold, green);
	drawText(painter, QRectF(70, 365, 900, 34), "MAP LIST", 20, QFont::Bold, muted);
	const int first = current >= 0 ? current : std::max(0, static_cast<int>(g_maps.size()) - 5);
	for (int row = 0; row < 5; ++row) {
		const int index = first + row;
		if (index >= static_cast<int>(g_maps.size())) break;
		const QRectF rowRect(70, 410 + row * 44, 1140, 36);
		if (index == current) roundedRect(painter, rowRect, 10, QColor(46, 235, 118, 35));
		const QString marker = g_maps[index].completed ? QStringLiteral("✓") : QString::number(index + 1).rightJustified(2, '0');
		drawText(painter, QRectF(84, rowRect.y(), 48, 36), marker, 20, QFont::Bold, g_maps[index].completed ? green : muted);
		drawText(painter, QRectF(145, rowRect.y(), 740, 36), g_maps[index].name, 23, QFont::DemiBold,
			 g_maps[index].completed ? muted : white);
		drawText(painter, QRectF(900, rowRect.y(), 285, 36), g_maps[index].code, 20, QFont::Normal, muted,
			 Qt::AlignRight | Qt::AlignVCenter);
	}
	const int total = static_cast<int>(g_maps.size());
	const qreal ratio = total > 0 ? static_cast<qreal>(completed) / total : 0.0;
	roundedRect(painter, QRectF(70, 646, 910, 10), 5, QColor(255, 255, 255, 28));
	if (ratio > 0.0) roundedRect(painter, QRectF(70, 646, 910 * ratio, 10), 5, green);
	drawText(painter, QRectF(1000, 626, 210, 46), QStringLiteral("%1 / %2 完了").arg(completed).arg(total), 22,
		 QFont::Bold, white, Qt::AlignRight | Qt::AlignVCenter);
	return image;
}

void refreshOverlay()
{
	QImage image = renderOverlay();
	QMutexLocker lock(&g_imageMutex);
	g_overlayImage = std::move(image);
	++g_imageRevision;
}

class RotationDock final : public QWidget {
	Q_OBJECT
public:
	RotationDock()
	{
		setObjectName("FortniteMapRotationDock");
		auto *root = new QVBoxLayout(this);
		auto *title = new QLabel(QStringLiteral("Fortnite クリエイティブ マップ順番"));
		title->setStyleSheet("font-size: 16px; font-weight: 700;");
		root->addWidget(title);
		auto *form = new QFormLayout;
		nameEdit_ = new QLineEdit;
		codeEdit_ = new QLineEdit;
		nameEdit_->setPlaceholderText(QStringLiteral("例：レッド vs ブルー"));
		codeEdit_->setPlaceholderText(QStringLiteral("例：1234-5678-9012"));
		form->addRow(QStringLiteral("マップ名"), nameEdit_);
		form->addRow(QStringLiteral("マップコード"), codeEdit_);
		root->addLayout(form);
		auto *addButton = new QPushButton(QStringLiteral("＋ マップを追加"));
		addButton->setDefault(true);
		root->addWidget(addButton);
		list_ = new QListWidget;
		root->addWidget(list_, 1);
		auto *row1 = new QHBoxLayout;
		auto *completeButton = new QPushButton(QStringLiteral("完了／未完了"));
		auto *upButton = new QPushButton(QStringLiteral("↑ 上へ"));
		auto *downButton = new QPushButton(QStringLiteral("↓ 下へ"));
		row1->addWidget(completeButton); row1->addWidget(upButton); row1->addWidget(downButton);
		root->addLayout(row1);
		auto *row2 = new QHBoxLayout;
		auto *deleteButton = new QPushButton(QStringLiteral("削除"));
		auto *resetButton = new QPushButton(QStringLiteral("全て未完了に戻す"));
		row2->addWidget(deleteButton); row2->addWidget(resetButton); root->addLayout(row2);
		auto *hint = new QLabel(QStringLiteral("ソース追加 →「Fortnite マップ順番」を選択してください。"));
		hint->setWordWrap(true); root->addWidget(hint);
		connect(addButton, &QPushButton::clicked, this, &RotationDock::addMap);
		connect(nameEdit_, &QLineEdit::returnPressed, this, &RotationDock::addMap);
		connect(codeEdit_, &QLineEdit::returnPressed, this, &RotationDock::addMap);
		connect(completeButton, &QPushButton::clicked, this, &RotationDock::toggleComplete);
		connect(deleteButton, &QPushButton::clicked, this, &RotationDock::removeMap);
		connect(resetButton, &QPushButton::clicked, this, &RotationDock::resetMaps);
		connect(upButton, &QPushButton::clicked, this, [this] { moveMap(-1); });
		connect(downButton, &QPushButton::clicked, this, [this] { moveMap(1); });
		rebuildList();
	}
private slots:
	void addMap()
	{
		const QString name = nameEdit_->text().trimmed(), code = codeEdit_->text().trimmed();
		if (name.isEmpty()) {
			QMessageBox::information(this, QStringLiteral("マップ名が必要です"), QStringLiteral("マップ名を入力してください。"));
			return;
		}
		g_maps.push_back({name, code, false}); nameEdit_->clear(); codeEdit_->clear();
		commit(static_cast<int>(g_maps.size()) - 1); nameEdit_->setFocus();
	}
	void toggleComplete()
	{
		const int row = list_->currentRow();
		if (row < 0 || row >= static_cast<int>(g_maps.size())) return;
		g_maps[row].completed = !g_maps[row].completed; commit(row);
	}
	void removeMap()
	{
		const int row = list_->currentRow();
		if (row < 0 || row >= static_cast<int>(g_maps.size())) return;
		g_maps.erase(g_maps.begin() + row); commit(std::min(row, static_cast<int>(g_maps.size()) - 1));
	}
	void resetMaps() { for (auto &map : g_maps) map.completed = false; commit(0); }
private:
	void moveMap(int direction)
	{
		const int row = list_->currentRow(), destination = row + direction;
		if (row < 0 || destination < 0 || destination >= static_cast<int>(g_maps.size())) return;
		std::swap(g_maps[row], g_maps[destination]); commit(destination);
	}
	void commit(int selected)
	{
		saveMaps(); refreshOverlay(); rebuildList();
		if (selected >= 0) list_->setCurrentRow(selected);
	}
	void rebuildList()
	{
		list_->clear();
		for (int i = 0; i < static_cast<int>(g_maps.size()); ++i) {
			const auto &map = g_maps[i];
			const QString state = map.completed ? QStringLiteral("✓") : QString::number(i + 1);
			list_->addItem(QStringLiteral("%1  %2   %3").arg(state, map.name, map.code));
		}
	}
	QLineEdit *nameEdit_ = nullptr;
	QLineEdit *codeEdit_ = nullptr;
	QListWidget *list_ = nullptr;
};

struct RotationSource { gs_texture_t *texture = nullptr; uint64_t revision = 0; };
const char *sourceName(void *) { return obs_module_text("Source.Name"); }
void *sourceCreate(obs_data_t *, obs_source_t *) { return new RotationSource; }
void sourceDestroy(void *data)
{
	auto *source = static_cast<RotationSource *>(data);
	obs_enter_graphics(); gs_texture_destroy(source->texture); obs_leave_graphics(); delete source;
}
uint32_t sourceWidth(void *) { return kCanvasWidth; }
uint32_t sourceHeight(void *) { return kCanvasHeight; }
void sourceRender(void *data, gs_effect_t *)
{
	auto *source = static_cast<RotationSource *>(data);
	QImage image; uint64_t revision;
	{
		QMutexLocker lock(&g_imageMutex); revision = g_imageRevision;
		if (source->revision != revision) image = g_overlayImage.copy();
	}
	if (!image.isNull()) {
		gs_texture_destroy(source->texture);
		const uint8_t *pixels = image.constBits();
		source->texture = gs_texture_create(kCanvasWidth, kCanvasHeight, GS_RGBA, 1, &pixels, 0);
		source->revision = revision;
	}
	if (!source->texture) return;
	gs_blend_state_push();
	gs_reset_blend_state();
	obs_source_draw(source->texture, 0, 0, kCanvasWidth, kCanvasHeight, false);
	gs_blend_state_pop();
}

obs_source_info rotationSourceInfo = {};
RotationDock *g_dock = nullptr;
} // namespace

bool obs_module_load(void)
{
	rotationSourceInfo.id = "fortnite_map_rotation_source";
	rotationSourceInfo.type = OBS_SOURCE_TYPE_INPUT;
	rotationSourceInfo.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB;
	rotationSourceInfo.get_name = sourceName;
	rotationSourceInfo.create = sourceCreate;
	rotationSourceInfo.destroy = sourceDestroy;
	rotationSourceInfo.get_width = sourceWidth;
	rotationSourceInfo.get_height = sourceHeight;
	rotationSourceInfo.video_render = sourceRender;
	obs_register_source(&rotationSourceInfo); loadMaps(); refreshOverlay();
	QTimer::singleShot(0, [] {
		g_dock = new RotationDock;
		if (!obs_frontend_add_dock_by_id("fortnite-map-rotation-dock", obs_module_text("Dock.Title"), g_dock)) {
			blog(LOG_ERROR, "Could not add the map rotation dock"); delete g_dock; g_dock = nullptr;
		}
	});
	blog(LOG_INFO, "Fortnite Map Rotation loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void) { blog(LOG_INFO, "Fortnite Map Rotation unloaded"); }
#include "plugin-main.moc"
