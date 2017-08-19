#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <fstream>
#include <sstream>
#include <QFileDialog>
#include <QImage>
#include <QGraphicsPixmapItem>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_spinBox_animfilenum_valueChanged(int arg1)
{
    if (arg1 < 1 || arg1 > 5)
        return;
    m_animfilenum = arg1;
    updateView();
}

void MainWindow::on_spinBox_idxindex_valueChanged(int arg1)
{
    if (arg1 < 0)
        return;
    m_idxindex = arg1;
    updateView();
}

void MainWindow::on_spinBox_framenumber_valueChanged(int arg1)
{
    if (arg1 < 0)
        return;
    m_framenum = arg1;
    updateView();
}

void MainWindow::resetView()
{
    ui->label_status_val->setText("Idle");

    ui->label_framecount_val->setText("-");
    ui->label_frameoffsetdec_val->setText("-");
    ui->label_frameoffsethex_val->setText("-");
    ui->label_frame_streampos_val->setText("-");
    ui->label_height_val->setText("-");
    ui->label_lookupdec_val->setText("-");
    ui->label_lookuphex_val->setText("-");
    ui->label_size_val->setText("-");
    ui->label_unknown_val->setText("-");
    ui->label_width_val->setText("-");
    ui->label_height_val->setText("-");
    ui->label_xcenter_val->setText("-");
    ui->label_ycenter_val->setText("-");

    if (ui->graphicsView->scene() != nullptr)
        delete ui->graphicsView->scene();
    QGraphicsScene* scene = new QGraphicsScene();
    ui->graphicsView->setScene(scene);
    ui->graphicsView->scene()->clear();
}

void MainWindow::on_pushButton_browse_clicked()
{
    QFileDialog *dlg = new QFileDialog();
    dlg->setOption(QFileDialog::ShowDirsOnly);
    dlg->setFileMode(QFileDialog::Directory);
    dlg->setAcceptMode(QFileDialog::AcceptOpen);
    dlg->setViewMode(QFileDialog::List);

    if (dlg->exec())
        ui->lineEdit->setText(dlg->directory().absolutePath());

    delete dlg;
}


void MainWindow::updateView()
{
    resetView();

    std::string basepath(ui->lineEdit->text().toStdString());
    std::replace(basepath.begin(), basepath.end(), '\\', '/');
    if (basepath[basepath.length()-1] != '/')
        basepath += '/';

    // Open *idx.mul
    std::string path_idx(basepath + "anim");
    if (m_animfilenum != 1)
        path_idx += std::to_string(m_animfilenum);
    path_idx += ".idx";
    std::ifstream fs_idx;
    fs_idx.open(path_idx, std::ifstream::in | std::ifstream::binary);
    if (!fs_idx.is_open())
    {
        ui->label_status_val->setText(QString("Can't open anim*.idx"));
        return;
    }

    // Look up in *idx.mul for the offset of the ID in *.mul
    // - Lookup:    size=4. Is either undefined ($FFFFFFFF -1) or the file offset in *.MUL
    // - Size:      size=4. Size of the pointed block.
    // - Unk:       size=4. Unknown
    uint32_t idx_lookup     = 0;
    uint32_t idx_size       = 0;
    uint32_t idx_unknown    = 0;
    fs_idx.seekg(m_idxindex * 12);

    fs_idx.read(reinterpret_cast<char*>(&idx_lookup), sizeof(uint32_t));
    ui->label_lookupdec_val->setText(QString::number(idx_lookup));
    ui->label_lookuphex_val->setText(QString("0x") + QString::number(idx_lookup, 16));

    fs_idx.read(reinterpret_cast<char*>(&idx_size), sizeof(uint32_t));
    ui->label_size_val->setText(QString::number(idx_size));

    fs_idx.read(reinterpret_cast<char*>(&idx_unknown), sizeof(uint32_t));
    ui->label_unknown_val->setText(QString::number(idx_unknown));

    fs_idx.close();


    //-------------------

    std::string path_anim(basepath + "anim");
    if (m_animfilenum != 1)
        path_anim += std::to_string(m_animfilenum);
    path_anim += ".mul";

    std::ifstream fs_anim;
    fs_anim.open(path_anim, std::ifstream::in | std::ifstream::binary);
    if (!fs_anim.is_open())
    {
        ui->label_status_val->setText(QString("Can't open anim*.mul"));
        return;
    }

    fs_anim.seekg(idx_lookup, std::ios_base::beg);


    /*
    AnimationGroup
        WORD[256] Palette
        DWORD FrameCount
        DWORD[FrameCount] FrameOffset
        Frames

    Seek from the end of Palette plus FrameOffset[FrameNum] bytes to find the start of Frame of interest

    Frame
        WORD xCenter
        WORD yCenter
        WORD Width
        WORD Height
        Chunks

    Chunk
        DWORD header;
        BYTE[xRun] palettePixels;

    See below for the data contained in the header (xOffset, yOffset and xRun).
    If the current chunk header is 0x7FFF7FFF, the image is completed.
    */

    int16_t palette[256];
    fs_anim.read(reinterpret_cast<char*>(palette), sizeof(palette));

    int32_t frame_count = 0;
    fs_anim.read(reinterpret_cast<char*>(&frame_count), sizeof(int32_t));
    if (m_framenum > frame_count)
    {
        m_framenum = 0;
        ui->spinBox_framenumber->setValue(m_framenum);
    }
    ui->label_framecount_val->setText(QString::number(frame_count));
    ui->spinBox_framenumber->setMaximum( (frame_count != 0) ? (frame_count - 1) : 0 );

    int32_t* frame_offsets = new int32_t[frame_count];
    fs_anim.read(reinterpret_cast<char*>(frame_offsets), sizeof(int32_t)*frame_count);
    ui->label_frameoffsetdec_val->setText(QString::number(frame_offsets[m_framenum]));
    ui->label_frameoffsethex_val->setText(QString("0x") + QString::number(frame_offsets[m_framenum], 16));

    fs_anim.seekg(idx_lookup + sizeof(palette) + frame_offsets[m_framenum], std::ios_base::beg);    // go to the selected frame

    size_t pos = fs_anim.tellg();
    ui->label_frame_streampos_val->setText(QString::number(pos));


    int16_t xCenter, yCenter;
    uint16_t width, height;

    fs_anim.read(reinterpret_cast<char*>(&xCenter), sizeof(int16_t));
    ui->label_xcenter_val->setText(QString::number(xCenter));

    fs_anim.read(reinterpret_cast<char*>(&yCenter), sizeof(int16_t));
    ui->label_ycenter_val->setText(QString::number(yCenter));

    fs_anim.read(reinterpret_cast<char*>(&width),   sizeof(uint16_t));
    ui->label_width_val->setText(QString::number(width));

    fs_anim.read(reinterpret_cast<char*>(&height),  sizeof(uint16_t));
    ui->label_height_val->setText(QString::number(height));

    if (height == 0 || width == 0 || height > 600 || width > 600)
    {
        ui->label_status_val->setText(QString("Invalid width/height"));
        fs_anim.close();
        return;
    }

    QImage* img = new QImage((int)width, (int)height, QImage::Format_ARGB32);
    img->fill(0);

    while ( !fs_anim.eof() && !fs_anim.fail() )
    {
        /*
        HEADER:

        1F	1E	1D	1C	1B	1A	19	18	17	16 | 15	14	13	12	11	10	0F	0E	0D	0C | 0B	0A	09	08	07	06	05	04	03	02	01	00
                xOffset (10 bytes)             |          yOffset (10 bytes)           |            xRun (12 bytes)
        The documentation(*) is wrong, because it inverted the position of x and y offsets. The one above is correct.
        *http://wpdev.sourceforge.net/docs/formats/csharp/animations.html

        xOffset and yOffset are relative to xCenter and yCenter.
        xOffset and yOffset are signed: see the compensation in the code below.
        xRun indicates how many pixels are contained in this line.

        For this piece of code, the MulPatcher source helped A LOT!
        */
        uint32_t header;
        fs_anim.read(reinterpret_cast<char*>(&header), 4);
        if ( header == 0x7FFF7FFF )
            break;

        uint32_t xRun = header & 0xFFF;              // take first 12 bytes
        uint32_t xOffset = (header >> 22) & 0x3FF;   // take 10 bytes
        uint32_t yOffset = (header >> 12) & 0x3FF;   // take 10 bytes
        // xOffset and yOffset are signed, so we need to compensate for that
        if (xOffset & 512)                  // 512 = 0x200
            xOffset |= (0xFFFFFFFF - 511);  // 511 = 0x1FF
        if (yOffset & 512)
            yOffset |= (0xFFFFFFFF - 511);

        int X = xOffset + xCenter;
        int Y = yOffset + yCenter + height;

        if (X < 0 || Y < 0 || Y >= height || X >= width)
            continue;

        for ( unsigned int k = 0; k < xRun; k++ )
        {
            uint8_t palette_index = 0;
            fs_anim.read(reinterpret_cast<char*>(&palette_index), 1);
            uint16_t color_argb16 = palette[palette_index];

            uint8_t a, r, g, b;
            a = 255;
            r = (uint8_t) (( color_argb16 & 0xF800) >> 10);
            g = (uint8_t) (( color_argb16 & 0x07E0) >> 5);
            b = (uint8_t) (( color_argb16 & 0x001F));
            r *= 8;
            g *= 8;
            b *= 8;
            uint32_t color_argb32 = (a << 24) | (r << 16) | (g << 8) | b;

            img->setPixel(X + k, Y, color_argb32);
        }
    }

    fs_anim.close();
    delete[] frame_offsets;

    QGraphicsPixmapItem* item = new QGraphicsPixmapItem(QPixmap::fromImage(*img));
    delete img;
    ui->graphicsView->scene()->addItem(item);
}

