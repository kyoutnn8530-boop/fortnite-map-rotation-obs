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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSet>
#include <QUrlQuery>
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
struct ChatRequest { QString author; QString name; QString code; };
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
	QFont font(QStringLiteral("Yu Gothic UI"));
	font.setPixelSize(size);
	font.setWeight(static_cast<QFont::Weight>(weight));
	painter.setFont(font);
	painter.setPen(color);
	painter.drawText(rect, alignment, text);
}

QImage renderOverlay()
{
	QImage image(kCanvasWidth, kCanvasHeight, QImage::Format_RGBA8888);
	image.fill(QColor(0, 255, 0));
	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setRenderHint(QPainter::TextAntialiasing, true);
	const QColor accent(78, 155, 94), panel(4, 57, 25, 242), white(242, 250, 244), muted(171, 197, 177);
	int current = -1, next = -1, completed = 0;
	for (int i = 0; i < static_cast<int>(g_maps.size()); ++i) {
		if (g_maps[i].completed) ++completed;
		else if (current < 0) current = i;
		else if (next < 0) next = i;
	}
	const int total = static_cast<int>(g_maps.size());
	const qreal ratio = total > 0 ? static_cast<qreal>(completed) / total : 0.0;

	const QRectF card(190, 78, 900, 410);
	roundedRect(painter, card, 38, panel);
	drawText(painter, QRectF(230, 96, 820, 50), QStringLiteral("フォートナイト  マップ順"), 32, QFont::Bold,
		 white, Qt::AlignCenter);
	drawText(painter, QRectF(230, 145, 820, 38), QStringLiteral("現在のマップ"), 23, QFont::DemiBold,
		 muted, Qt::AlignCenter);
	drawText(painter, QRectF(230, 182, 820, 92),
		 current < 0 ? QStringLiteral("マップ未登録") : g_maps[current].name, 54, QFont::Black, white,
		 Qt::AlignCenter);
	if (current >= 0 && !g_maps[current].code.isEmpty())
		drawText(painter, QRectF(230, 268, 820, 42), g_maps[current].code, 25, QFont::DemiBold, muted,
			 Qt::AlignCenter);
	const QString nextText = next < 0 ? QStringLiteral("次のマップ：—")
					  : QStringLiteral("次のマップ：%1").arg(g_maps[next].name);
	drawText(painter, QRectF(230, 315, 820, 50), nextText, 29, QFont::Bold, white, Qt::AlignCenter);
	roundedRect(painter, QRectF(190, 391, 900, 12), 6, QColor(104, 167, 116, 80));
	if (ratio > 0.0)
		roundedRect(painter, QRectF(190, 391, 900 * ratio, 12), 6, accent);
	drawText(painter, QRectF(230, 414, 820, 48), QStringLiteral("%1 / %2 マップ完了").arg(completed).arg(total),
		 22, QFont::DemiBold, muted, Qt::AlignCenter);
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

		auto *youtubeTitle = new QLabel(QStringLiteral("YouTubeチャット連携"));
		youtubeTitle->setStyleSheet("font-size: 14px; font-weight: 700; margin-top: 8px;");
		root->addWidget(youtubeTitle);
		auto *youtubeForm = new QFormLayout;
		apiKeyEdit_ = new QLineEdit;
		apiKeyEdit_->setEchoMode(QLineEdit::Password);
		apiKeyEdit_->setPlaceholderText(QStringLiteral("YouTube Data APIキー"));
		videoEdit_ = new QLineEdit;
		videoEdit_->setPlaceholderText(QStringLiteral("配信URL または 動画ID"));
		youtubeForm->addRow(QStringLiteral("APIキー"), apiKeyEdit_);
		youtubeForm->addRow(QStringLiteral("配信"), videoEdit_);
		root->addLayout(youtubeForm);
		auto *monitorRow = new QHBoxLayout;
		monitorButton_ = new QPushButton(QStringLiteral("チャット監視を開始"));
		statusLabel_ = new QLabel(QStringLiteral("停止中"));
		monitorRow->addWidget(monitorButton_);
		monitorRow->addWidget(statusLabel_, 1);
		root->addLayout(monitorRow);
		root->addWidget(new QLabel(QStringLiteral("承認待ち（「マップ希望 マップ名 1234-5678-9012」）")));
		pendingList_ = new QListWidget;
		pendingList_->setMaximumHeight(130);
		root->addWidget(pendingList_);
		auto *approvalRow = new QHBoxLayout;
		auto *approveButton = new QPushButton(QStringLiteral("承認して追加"));
		auto *rejectButton = new QPushButton(QStringLiteral("却下"));
		approvalRow->addWidget(approveButton);
		approvalRow->addWidget(rejectButton);
		root->addLayout(approvalRow);

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
		connect(monitorButton_, &QPushButton::clicked, this, &RotationDock::toggleMonitoring);
		connect(approveButton, &QPushButton::clicked, this, &RotationDock::approveRequest);
		connect(rejectButton, &QPushButton::clicked, this, &RotationDock::rejectRequest);
		connect(&pollTimer_, &QTimer::timeout, this, &RotationDock::pollChat);
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
	void toggleMonitoring()
	{
		if (monitoring_) { stopMonitoring(QStringLiteral("停止中")); return; }
		const QString key = apiKeyEdit_->text().trimmed();
		QString video = videoEdit_->text().trimmed();
		QRegularExpression idPattern(QStringLiteral("(?:v=|youtu\\.be/|live/)([A-Za-z0-9_-]{11})"));
		const auto match = idPattern.match(video);
		if (match.hasMatch()) video = match.captured(1);
		if (!QRegularExpression(QStringLiteral("^[A-Za-z0-9_-]{11}$")).match(video).hasMatch() || key.isEmpty()) {
			QMessageBox::information(this, QStringLiteral("入力を確認"),
				QStringLiteral("YouTube Data APIキーと配信URL（または11文字の動画ID）を入力してください。"));
			return;
		}
		apiKey_ = key; videoId_ = video; nextPageToken_.clear(); liveChatId_.clear(); firstPoll_ = true;
		monitoring_ = true; monitorButton_->setText(QStringLiteral("監視を停止"));
		statusLabel_->setText(QStringLiteral("配信を確認中…"));
		QUrl url(QStringLiteral("https://www.googleapis.com/youtube/v3/videos"));
		QUrlQuery query; query.addQueryItem(QStringLiteral("part"), QStringLiteral("liveStreamingDetails"));
		query.addQueryItem(QStringLiteral("id"), videoId_); query.addQueryItem(QStringLiteral("key"), apiKey_);
		url.setQuery(query);
		auto *reply = network_.get(QNetworkRequest(url));
		connect(reply, &QNetworkReply::finished, this, [this, reply] {
			const QByteArray body = reply->readAll(); const auto error = reply->error(); reply->deleteLater();
			if (error != QNetworkReply::NoError) { stopMonitoring(QStringLiteral("配信情報の取得に失敗")); return; }
			const QJsonArray items = QJsonDocument::fromJson(body).object()[QStringLiteral("items")].toArray();
			if (items.isEmpty()) { stopMonitoring(QStringLiteral("配信が見つかりません")); return; }
			liveChatId_ = items[0].toObject()[QStringLiteral("liveStreamingDetails")].toObject()
				[QStringLiteral("activeLiveChatId")].toString();
			if (liveChatId_.isEmpty()) { stopMonitoring(QStringLiteral("ライブチャットがありません")); return; }
			statusLabel_->setText(QStringLiteral("接続中（キーワード：マップ希望）")); pollChat();
		});
	}
	void pollChat()
	{
		if (!monitoring_ || liveChatId_.isEmpty()) return;
		QUrl url(QStringLiteral("https://www.googleapis.com/youtube/v3/liveChat/messages"));
		QUrlQuery query; query.addQueryItem(QStringLiteral("part"), QStringLiteral("snippet,authorDetails"));
		query.addQueryItem(QStringLiteral("liveChatId"), liveChatId_);
		query.addQueryItem(QStringLiteral("maxResults"), QStringLiteral("200"));
		query.addQueryItem(QStringLiteral("key"), apiKey_);
		if (!nextPageToken_.isEmpty()) query.addQueryItem(QStringLiteral("pageToken"), nextPageToken_);
		url.setQuery(query);
		auto *reply = network_.get(QNetworkRequest(url));
		connect(reply, &QNetworkReply::finished, this, [this, reply] {
			const QByteArray body = reply->readAll(); const auto error = reply->error(); reply->deleteLater();
			if (!monitoring_) return;
			if (error != QNetworkReply::NoError) { stopMonitoring(QStringLiteral("チャット取得エラー")); return; }
			const QJsonObject root = QJsonDocument::fromJson(body).object();
			const QJsonArray items = root[QStringLiteral("items")].toArray();
			if (!firstPoll_) {
				for (const QJsonValue &value : items) {
					const QJsonObject item = value.toObject();
					const QString messageId = item[QStringLiteral("id")].toString();
					if (seenMessageIds_.contains(messageId)) continue;
					seenMessageIds_.insert(messageId);
					const QJsonObject snippet = item[QStringLiteral("snippet")].toObject();
					const QString message = snippet[QStringLiteral("displayMessage")].toString().trimmed();
					if (!message.contains(QStringLiteral("マップ希望"))) continue;
					QRegularExpression codePattern(QStringLiteral("(\\d{4})[- ]?(\\d{4})[- ]?(\\d{4})"));
					const auto codeMatch = codePattern.match(message);
					if (!codeMatch.hasMatch()) continue;
					const QString code = codeMatch.captured(1) + "-" + codeMatch.captured(2) + "-" + codeMatch.captured(3);
					bool duplicate = false;
					for (const auto &map : g_maps) if (map.code == code) duplicate = true;
					for (const auto &request : pending_) if (request.code == code) duplicate = true;
					if (duplicate) continue;
					QString name = message; name.remove(QStringLiteral("マップ希望")); name.remove(codeMatch.captured(0)); name = name.trimmed();
					const QString author = item[QStringLiteral("authorDetails")].toObject()[QStringLiteral("displayName")].toString();
					if (name.isEmpty()) name = QStringLiteral("%1さんの希望").arg(author);
					pending_.push_back({author, name, code});
					pendingList_->addItem(QStringLiteral("%1｜%2｜%3").arg(author, name, code));
				}
			}
			firstPoll_ = false;
			nextPageToken_ = root[QStringLiteral("nextPageToken")].toString();
			const int interval = std::clamp(root[QStringLiteral("pollingIntervalMillis")].toInt(5000), 1000, 30000);
			pollTimer_.start(interval);
		});
	}
	void approveRequest()
	{
		const int row = pendingList_->currentRow();
		if (row < 0 || row >= static_cast<int>(pending_.size())) return;
		const auto request = pending_[row];
		g_maps.push_back({request.name, request.code, false});
		pending_.erase(pending_.begin() + row); delete pendingList_->takeItem(row);
		commit(static_cast<int>(g_maps.size()) - 1);
	}
	void rejectRequest()
	{
		const int row = pendingList_->currentRow();
		if (row < 0 || row >= static_cast<int>(pending_.size())) return;
		pending_.erase(pending_.begin() + row); delete pendingList_->takeItem(row);
	}
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
	void stopMonitoring(const QString &status)
	{
		monitoring_ = false; pollTimer_.stop(); monitorButton_->setText(QStringLiteral("チャット監視を開始"));
		statusLabel_->setText(status); liveChatId_.clear(); nextPageToken_.clear();
	}
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
	QNetworkAccessManager network_{this};
	QTimer pollTimer_;
	QLineEdit *apiKeyEdit_ = nullptr;
	QLineEdit *videoEdit_ = nullptr;
	QPushButton *monitorButton_ = nullptr;
	QLabel *statusLabel_ = nullptr;
	QListWidget *pendingList_ = nullptr;
	QString apiKey_, videoId_, liveChatId_, nextPageToken_;
	QSet<QString> seenMessageIds_;
	std::vector<ChatRequest> pending_;
	bool monitoring_ = false, firstPoll_ = true;
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
