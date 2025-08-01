﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MULTIMEDIASOURCEMUXER_H
#define ZLMEDIAKIT_MULTIMEDIASOURCEMUXER_H

#include "Common/Stamp.h"
#include "Common/MediaSource.h"
#include "Common/MediaSink.h"
#include "Record/Recorder.h"
#include "Rtp/RtpSender.h"
#include "Record/HlsRecorder.h"
#include "Record/HlsMediaSource.h"
#include "Rtsp/RtspMediaSourceMuxer.h"
#include "Rtmp/RtmpMediaSourceMuxer.h"
#include "TS/TSMediaSourceMuxer.h"
#include "FMP4/FMP4MediaSourceMuxer.h"

namespace mediakit {

class MultiMediaSourceMuxer : public MediaSourceEventInterceptor, public MediaSink, public std::enable_shared_from_this<MultiMediaSourceMuxer>{
public:
    using Ptr = std::shared_ptr<MultiMediaSourceMuxer>;
    using RingType = toolkit::RingBuffer<Frame::Ptr>;

    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void onAllTrackReady() = 0;
    };

    MultiMediaSourceMuxer(const MediaTuple& tuple, float dur_sec = 0.0,const ProtocolOption &option = ProtocolOption());

    /**
     * 设置事件监听器
     * @param listener 监听器
     * Set event listener
     * @param listener Listener
     
     * [AUTO-TRANSLATED:d829419b]
     */
    void setMediaListener(const std::weak_ptr<MediaSourceEvent> &listener);

     /**
      * 设置Track就绪事件监听器
      * @param listener 事件监听器
      * Set Track ready event listener
      * @param listener Event listener
      
      * [AUTO-TRANSLATED:64262ac5]
     */
    void setTrackListener(const std::weak_ptr<Listener> &listener);

    /**
     * 返回总的消费者个数
     * Return the total number of consumers
     
     * [AUTO-TRANSLATED:5eaac131]
     */
    int totalReaderCount() const;

    /**
     * 判断是否生效(是否正在转其他协议)
     * Determine whether it is effective (whether it is being converted to another protocol)
     
     * [AUTO-TRANSLATED:ca92165c]
     */
    bool isEnabled();

    /**
     * 设置MediaSource时间戳
     * @param stamp 时间戳
     * Set MediaSource timestamp
     * @param stamp Timestamp
     
     * [AUTO-TRANSLATED:a75cc2fa]
     */
    void setTimeStamp(uint32_t stamp);

    /**
     * 重置track
     * Reset track
     
     * [AUTO-TRANSLATED:95dc0b4f]
     */
    void resetTracks() override;

    /////////////////////////////////MediaSourceEvent override/////////////////////////////////

    /**
     * 观看总人数
     * @param sender 事件发送者
     * @return 观看总人数
     * Total number of viewers
     * @param sender Event sender
     * @return Total number of viewers
     
     * [AUTO-TRANSLATED:f4d7146c]
     */
    int totalReaderCount(MediaSource &sender) override;

    /**
     * 设置录制状态
     * @param type 录制类型
     * @param start 开始或停止
     * @param custom_path 开启录制时，指定自定义路径
     * @return 是否设置成功
     * Set recording status
     * @param type Recording type
     * @param start Start or stop
     * @param custom_path Specify a custom path when recording is enabled
     * @return Whether the setting is successful
     
     * [AUTO-TRANSLATED:cb1fd8a9]
     */
    bool setupRecord(MediaSource &sender, Recorder::type type, bool start, const std::string &custom_path, size_t max_second) override;

    /**
     * 开始录制mp4
     * @param file_path mp4相对路径
     * @param back_time_ms 回溯录制时长
     * @param forward_time_ms 后续录制时长
     * @return 录制文件绝对路径
     */
    std::string startRecord(const std::string &file_path, uint32_t back_time_ms, uint32_t forward_time_ms);

    /**
     * 获取录制状态
     * @param type 录制类型
     * @return 录制状态
     * Get recording status
     * @param type Recording type
     * @return Recording status
     
     * [AUTO-TRANSLATED:798afa71]
     */
    bool isRecording(MediaSource &sender, Recorder::type type) override;

    /**
     * 开始发送ps-rtp流
     * @param dst_url 目标ip或域名
     * @param dst_port 目标端口
     * @param ssrc rtp的ssrc
     * @param is_udp 是否为udp
     * @param cb 启动成功或失败回调
     * Start sending ps-rtp stream
     * @param dst_url Target ip or domain name
     * @param dst_port Target port
     * @param ssrc rtp's ssrc
     * @param is_udp Whether it is udp
     * @param cb Start success or failure callback
     
     * [AUTO-TRANSLATED:620416c2]
     */
    void startSendRtp(MediaSource &sender, const MediaSourceEvent::SendRtpArgs &args, const std::function<void(uint16_t, const toolkit::SockException &)> cb) override;

    /**
     * 停止ps-rtp发送
     * @return 是否成功
     * Stop ps-rtp sending
     * @return Whether it is successful
     
     * [AUTO-TRANSLATED:b91e2055]
     */
    bool stopSendRtp(MediaSource &sender, const std::string &ssrc) override;

    /**
     * 获取所有Track
     * @param trackReady 是否筛选过滤未就绪的track
     * @return 所有Track
     * Get all Tracks
     * @param trackReady Whether to filter out unready tracks
     * @return All Tracks
     
     * [AUTO-TRANSLATED:53755f5d]
     */
    std::vector<Track::Ptr> getMediaTracks(MediaSource &sender, bool trackReady = true) const override;

    /**
     * 获取所属线程
     * Get the thread it belongs to
     
     * [AUTO-TRANSLATED:a4dc847e]
     */
    toolkit::EventPoller::Ptr getOwnerPoller(MediaSource &sender) override;
    
    /**
     * 关闭流
     * @return 是否成功
     */
    bool close(MediaSource &sender) override;

    /**
     * 获取本对象
     * Get this object
     
     * [AUTO-TRANSLATED:5e119bb3]
     */
    std::shared_ptr<MultiMediaSourceMuxer> getMuxer(MediaSource &sender) const override;

    const ProtocolOption &getOption() const;
    const MediaTuple &getMediaTuple() const;
    std::string shortUrl() const;

    void forEachRtpSender(const std::function<void(const std::string &ssrc, const RtpSender &sender)> &cb) const;

protected:
    /////////////////////////////////MediaSink override/////////////////////////////////

    /**
    * 某track已经准备好，其ready()状态返回true，
    * 此时代表可以获取其例如sps pps等相关信息了
    * @param track
     * A certain track is ready, its ready() status returns true,
     * This means that you can get information such as sps pps, etc.
     * @param track
     
     * [AUTO-TRANSLATED:05659d48]
    */
    bool onTrackReady(const Track::Ptr & track) override;

    /**
     * 所有Track已经准备好，
     * All Tracks are ready,
     
     * [AUTO-TRANSLATED:c54d02e2]
     */
    void onAllTrackReady() override;

    /**
     * 某Track输出frame，在onAllTrackReady触发后才会调用此方法
     * @param frame
     * A certain Track outputs a frame, this method will be called after onAllTrackReady is triggered
     * @param frame
     
     * [AUTO-TRANSLATED:debbd247]
     */
    bool onTrackFrame(const Frame::Ptr &frame) override;
    bool onTrackFrame_l(const Frame::Ptr &frame);

private:
    void createGopCacheIfNeed(size_t gop_count);
    std::shared_ptr<MediaSinkInterface> makeRecorder(MediaSource &sender, Recorder::type type);

private:
    bool _is_enable = false;
    bool _create_in_poller = false;
    bool _video_key_pos = false;
    float _dur_sec;
    std::shared_ptr<class FramePacedSender> _paced_sender;
    MediaTuple _tuple;
    ProtocolOption _option;
    toolkit::Ticker _last_check;
    std::unordered_map<int, Stamp> _stamps;
    std::weak_ptr<Listener> _track_listener;
    std::unordered_multimap<std::string, std::tuple<RingType::RingReader::Ptr, std::weak_ptr<RtpSender>>> _rtp_sender;
    FMP4MediaSourceMuxer::Ptr _fmp4;
    RtmpMediaSourceMuxer::Ptr _rtmp;
    RtspMediaSourceMuxer::Ptr _rtsp;
    TSMediaSourceMuxer::Ptr _ts;
    MediaSinkInterface::Ptr _mp4;
    HlsRecorder::Ptr _hls;
    HlsFMP4Recorder::Ptr _hls_fmp4;
    toolkit::EventPoller::Ptr _poller;
    RingType::Ptr _ring;

    // 对象个数统计  [AUTO-TRANSLATED:3b43e8c2]
    // Object count statistics
    toolkit::ObjectStatistic<MultiMediaSourceMuxer> _statistic;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_MULTIMEDIASOURCEMUXER_H
