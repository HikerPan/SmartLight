var LocalTime = 0;
$(document).ready(function() {

	getDate(); 	
	setInterval(getMachineInfo, 5000)//定时刷新 ID信息
	setInterval(DateDisplay, 1000);//定时刷新时间
	setInterval(getConfig, 5000);//定时锁车信息和配置信息
	setInterval(getWorkInfo, 5000);//定时工作信息
	getEcharts(1000);//定时刷新仪表盘
	$("#date").click(getDate); //点击刷新时间
});
function SetFormat(s) {
	if (s < 10)
		s = "0" + s;
	return s;
}
function FormatDate(nS) {
	var week = new Array();
	week[0] = "Sunday";
	week[1] = "Monday";
	week[2] = "Tuesday";
	week[3] = "Wednesday";
	week[4] = "Thurday";
	week[5] = "Friday";
	week[6] = "Saturday";
	var today = new Date(parseInt(nS) * 1000);
	var y = SetFormat(today.getFullYear());
	var M = SetFormat(today.getMonth() + 1);
	var d = SetFormat(today.getDate());
	var h = SetFormat(today.getHours());
	var m = SetFormat(today.getMinutes());
	var s = SetFormat(today.getSeconds());
	var w = today.getDay();
	return y + "-" + M + "-" + d + " " + h + ":" + m + ":" + s + " " + week[w];
}
function DateDisplay() {
	LocalTime++;
	$("#date").text(FormatDate(LocalTime));
}
function getDate() {
			var today = new Date();
			LocalTime = today.getTime()/ 1000;	 	
}
// 点击锁车或者解锁
function machineControl(type,value) {
			var url = "./control";
           if(value == true){
           	  Ewin.confirm({ message: "Are you sure you want to lock the car?" }).on(function (e) {
                if (!e) {
                    return;
                }else{
		           ajaxToBackstage(url,type,value);             	
		        }                
            });
           }else{ 
           	  Ewin.confirm({ message: "Are you sure you want to unlock the car?" }).on(function (e) {
                if (!e) {
                    return;
                }else{
                	ajaxToBackstage(url,type,value);
                }            
            });
          }

}
function getMachineInfo() { //获取终端工作信息
		$.get("./getMachineInfo", function(data){
	  		if(data!=null&&data!=undefined){
	  			$("#ChassID").html("ChassID:"+data.ChassID);
	  			$("#DeviceID").html("DeviceID:"+data.DeviceID);
	  			
	  		}else{
	 			alert("获取设备信息为null!");
			 }
	},"json");
}
// 配置终端上传信息周期
function machineConfig(type) {		
			var url = "./config";
           var value = $("#"+type).val();
           if(value==null||value==undefined||value==""){
           	alert("parameter cannot be empty!");
           	return;
           }
           if(value.length>9){
           	alert("parameter cannot be too large!");
           	return;
           }
           if(value==0){
           	alert("parameter cannot be 0!");
           	return;
           }
           if(!isPositiveInteger(value)){
           	alert("parameter must be a positive integer!")
           	return;
           }
           
       	  Ewin.confirm({ message: "Are you sure you want to configure this parameter?" }).on(function (e) {
            if (!e) {
                return;
            }else{
            	ajaxToBackstage(url,type,value);
            }            
       		});
}
function ajaxToBackstage(url,type,value){//向后台传值
    $.ajax({
        type:"post",
        url: url,
        data:'{ "type":"'+type+'","value":'+value+'}',
        dataType:"json",
        contentType: "application/json;charset=utf-8",
        success: function (data) {
        	alert(data.msg);
        }
    })
}
function getConfig() {  //获取终端配置
	$.get("./getConfig", function(data){
	 if(data!=null&&data!=undefined){
	 	if(data.lockMachine){
	 		$("#lockMachineState").html("锁车");
	 	}else{
	 		$("#lockMachineState").html("未锁车");
	 	}
	 	if(data.powerlineCutLock){
	 		$("#powerlineCutLockState").html("锁车");
	 	}else{
	 		$("#powerlineCutLockState").html("未锁车");
	 	}
	 	if(data.gpsCableCutLock){
	 		$("#gpsCableCutLockState").html("锁车");
	 	}else{
	 		$("#gpsCableCutLockState").html("未锁车");
	 	}
	 	if(data.geoFenceLock){
	 		$("#geoFenceLockState").html("锁车");
	 	}else{
	 		$("#geoFenceLockState").html("未锁车");
	 	}
	 	if(data.timeFenceLock){
	 		$("#timeFenceLockState").html("锁车");
	 	}else{
	 		$("#timeFenceLockState").html("未锁车");
	 	}
		if(data.openCoverLock){
	 		$("#openCoverLockState").html("锁车");
	 	}else{
	 		$("#openCoverLockState").html("未锁车");
	 	}
	 	$("#firstWarnDaysValue").html(data.firstWarnDays);
	 	$("#secondWarnDaysValue").html(data.secondWarnDays);
	 	$("#engStopDaysValue").html(data.engStopDays);
	 	$("#canMsgUpdateIntervalValue").html(data.canMsgUpdateInterval);
	 	$("#devMsgUpdateIntervalValue").html(data.devMsgUpdateInterval);
	 	$("#dtcUpdateIntervalValue").html(data.dtcUpdateInterval);
	 }else{
	 	alert("获取终端配置失败!");
	 }
	},"json");
}
function getWorkInfo() { //获取终端工作信息
		$.get("./getWorkInfo", function(data){
	  		if(data!=null&&data!=undefined){
	  			$("#vehicleBattery").html(data.vehicleBattery+" V");
	  			$("#moduleBattery").html(data.moduleBattery+" %");
	  			$("#acc").html(data.ACC);
	  			$("#start").html(data.Start);	  			
	  			$("#gpsCableState").html(data.gpsCable);
	  			$("#locateMode").html(data.GPS_Status);
	  			$("#fourGSignalNo").html(data.LTE_Signal+" dB");	   			
	  			$("#wifiSignalNo").html(data.WIFI_Signal+" dB");	  			
	  			$("#lockState").html(data.LockState);
	  			$("#canBusState").html(data.canBusState);
	  			$("#currentFault").html(data.currentFaul);
	  			$("#workHours").html(data.workHours+" H");
	  		}else{
	 			alert("获取终端工作信息为null!");
			 }
	},"json");
}
function getEcharts(senconds) {//设置仪表盘
			var oilDom = document.getElementById("oilChart");
			var speedDom = document.getElementById("speedChart");
			var temperatureDom = document.getElementById("temperatureChart");
			var oilChart = echarts.init(oilDom);
			var speedChart = echarts.init(speedDom);
			var temperatureChart = echarts.init(temperatureDom);
			var oilOption = {
				backgroundColor: '#fff',
				series: [
					{
						name: '油表',
						type: 'gauge',
						center: ['75%', '50%'], // 仪表盘位置(圆心坐标)
						radius: '90%',
						min: 0,
						max: 2,
						startAngle: 180,// 仪表盘起始角度,默认 225。圆心 正右手侧为0度，正上方为90度，正左手侧为180度。
						endAngle: 90,// 仪表盘结束角度,默认 -45
						splitNumber: 2,
						axisLine: { // 坐标轴线
							lineStyle: { // 属性lineStyle控制线条样式
								width: 8,
								color: [
									[0.5, '#3483c8'],
									[0.82, '#f8c047'],
									[1, '#ee8389']
								]
							}
						},
						axisTick: { // 坐标轴小标记
							splitNumber: 5,
							length: 10, // 属性length控制线长
							lineStyle: { // 属性lineStyle控制线条样式
								color: 'auto'
							}
						},
						axisLabel: {
							formatter: function(v) {
								switch (v + '') {
									case '0':
										return 'E';
									case '1':
										return 'Gas';
									case '2':
										return 'F';
								}
							}
						},
						splitLine: { // 分隔线
							length: 15, // 属性length控制线长
							lineStyle: { // 属性lineStyle（详见lineStyle）控制线条样式
								color: 'auto'
							}
						},
						pointer: {
							width: 2
							
						},
						title: {
							show: false
						},
						detail: {
							/*show: false*/
							fontSize: 14,
							color: '#63869e'
						},
						data: [{
							value: 0,//数据
							name: 'gas'
						}]
					}
				]
			};			
			var speedOption = {
				backgroundColor: '#fff',
				series: [				
					{
						name: '转速',
						type: 'gauge',
						z: 3,
						min: 0,
						max: 7,
						startAngle: 180,
						endAngle: 0,
						splitNumber: 7,
						center: ['50%', '50%'],
						radius: '100%',
						axisLine: { 
							lineStyle: { 
								width: 10,
								color: [
									[0.5, '#3483c8'],
									[0.82, '#f8c047'],
									[1, '#ee8389']
								]
							}
						},
						axisTick: { //刻度
							length: 15, 
							lineStyle: { 
								color: 'auto'
							}
						},
						splitLine: { 
							length: 20, 
							lineStyle: { 
								color: 'auto'
							}
						},
						axisLabel: {
							backgroundColor: 'transparent',
							borderRadius: 2,
							color: '#000',
							fontSize: 12,
							padding: 3,
							textShadowBlur: 2
						},
						title: {
							fontWeight: 'bolder',
							fontSize: 12,
							fontStyle: 'italic',
							offsetCenter: [0, '-30%'] 
						},
						detail: {
							/*formatter: function(value) {
								value = value/1000;
								
							},*/
							fontWeight: 'bolder',
							fontSize: 20,
							shadowBlur: 5,
							shadowOffsetX: 0,
							shadowOffsetY: 3,
							textBorderWidth: 2,
							textShadowBlur: 2,
							textShadowOffsetX: 0,
							textShadowOffsetY: 0,
							fontFamily: 'Arial',
							width: 100,
							height: 25,
							color: '#63869e',
							rich: {}
						},
						data: [{
							value: 0,
							name: 'x1000 r/min'
						}]
					}
				]
			};			
			var temperatureOption = {
				backgroundColor: '#fff',
				series: [
					{
						name: '冷却液温度',
						type: 'gauge',
						center: ['25%', '50%'], 
						radius: '90%',
						min: 0,
						max: 150,
						startAngle: 90,
						endAngle: 0,
						splitNumber: 5,
						axisLine: { 
							lineStyle: { 
								width: 8,
								color: [
									[0.5, '#3483c8'],
									[0.82, '#f8c047'],
									[1, '#ee8389']
								]
							}
						},
						axisTick: { 
							length: 8, 
							lineStyle: { 
								color: 'auto'
							}
						},
						splitLine: { 
							length: 10, 
							lineStyle: { 
								color: 'auto'
							}
						},
						pointer: {
							width: 5
						},
						title: {
							offsetCenter: [0, '-30%'] 
						},
						detail: {
							/*fontWeight: 'bolder',
							fontSize: 20,
							color: 'transparent'*/
							offsetCenter: [-20, '50%'], // x, y，单位px
							fontSize: 14,
							color: '#63869e'
						},
						data: [{
							value: 0,
							name: '℃'
						}]
					}
				]
			};
			oilChart.setOption(oilOption, true);
			speedChart.setOption(speedOption, true);
			temperatureChart.setOption(temperatureOption, true);
			$.get("./getCharts", function(data){
				if(data!=null&&data!=undefined){
					oilOption.series[0].data[0].value = data.oil;
					speedOption.series[0].data[0].value = data.rpm/1000;
					temperatureOption.series[0].data[0].value = data.temperature;
					oilChart.setOption(oilOption, true);
					speedChart.setOption(speedOption, true);
					temperatureChart.setOption(temperatureOption, true);
				}else{
					alert("获取仪表数据内容为空！");
				}
			},"json");
			setInterval(function() {				
				$.get("./getCharts", function(data){
					if(data!=null&&data!=undefined){
						oilOption.series[0].data[0].value = data.oil;
						speedOption.series[0].data[0].value = data.rpm/1000;
						temperatureOption.series[0].data[0].value = data.temperature;
						oilChart.setOption(oilOption, true);
						speedChart.setOption(speedOption, true);
						temperatureChart.setOption(temperatureOption, true);
					}else{
						alert("获取仪表数据内容为空！");
					}
				},"json");

			}, senconds);	
}
function isPositiveInteger(s){//是否为正整数
     var re = /^[0-9]+$/ ;
     return re.test(s)
 }
//确认框组件
(function ($) {
    window.Ewin = function () {
        var html = '<div id="[Id]" class="modal fade" role="dialog" aria-labelledby="modalLabel">' +
                              '<div class="modal-dialog modal-sm">' +
                                  '<div class="modal-content">' +
                                      '<div class="modal-header">' +
                                          '<button type="button" class="close" data-dismiss="modal"><span aria-hidden="true">&times;</span><span class="sr-only">Close</span></button>' +
                                          '<h4 class="modal-title" id="modalLabel">[Title]</h4>' +
                                      '</div>' +
                                      '<div class="modal-body">' +
                                      '<p>[Message]</p>' +
                                      '</div>' +
                                       '<div class="modal-footer">' +
        '<button type="button" class="btn btn-default cancel" data-dismiss="modal">[BtnCancel]</button>' +
        '<button type="button" class="btn btn-primary ok" data-dismiss="modal">[BtnOk]</button>' +
    '</div>' +
                                  '</div>' +
                              '</div>' +
                          '</div>';
        var dialogdHtml = '<div id="[Id]" class="modal fade" role="dialog" aria-labelledby="modalLabel">' +
                              '<div class="modal-dialog">' +
                                  '<div class="modal-content">' +
                                      '<div class="modal-header">' +
                                          '<button type="button" class="close" data-dismiss="modal"><span aria-hidden="true">&times;</span><span class="sr-only">Close</span></button>' +
                                          '<h4 class="modal-title" id="modalLabel">[Title]</h4>' +
                                      '</div>' +
                                      '<div class="modal-body">' +
                                      '</div>' +
                                  '</div>' +
                              '</div>' +
                          '</div>';
        var reg = new RegExp("\\[([^\\[\\]]*?)\\]", 'igm');
        var generateId = function () {
            var date = new Date();
            return 'mdl' + date.valueOf();
        }
        var init = function (options) {
            options = $.extend({}, {
                title: "Tips",
                message: "content",
                btnok: "OK",
                btncl: "Cancel",
                width: 200,
                auto: false
            }, options || {});
            var modalId = generateId();
            var content = html.replace(reg, function (node, key) {
                return {
                    Id: modalId,
                    Title: options.title,
                    Message: options.message,
                    BtnOk: options.btnok,
                    BtnCancel: options.btncl
                }[key];
            });
            $('body').append(content);
            $('#' + modalId).modal({
                width: options.width,
                backdrop: 'static'
            });
            $('#' + modalId).on('hide.bs.modal', function (e) {
                $('body').find('#' + modalId).remove();
            });
            return modalId;
        }
        return {
            alert: function (options) {
                if (typeof options == 'string') {
                    options = {
                        message: options
                    };
                }
                var id = init(options);
                var modal = $('#' + id);
                modal.find('.ok').removeClass('btn-success').addClass('btn-primary');
                modal.find('.cancel').hide();
                return {
                    id: id,
                    on: function (callback) {
                        if (callback && callback instanceof Function) {
                            modal.find('.ok').click(function () { callback(true); });
                        }
                    },
                    hide: function (callback) {
                        if (callback && callback instanceof Function) {
                            modal.on('hide.bs.modal', function (e) {
                                callback(e);
                            });
                        }
                    }
                };
            },
            confirm: function (options) {
                var id = init(options);
                var modal = $('#' + id);
                modal.find('.ok').removeClass('btn-primary').addClass('btn-success');
                modal.find('.cancel').show();
                return {
                    id: id,
                    on: function (callback) {
                        if (callback && callback instanceof Function) {
                            modal.find('.ok').click(function () { callback(true); });
                            modal.find('.cancel').click(function () { callback(false); });
                        }
                    },
                    hide: function (callback) {
                        if (callback && callback instanceof Function) {
                            modal.on('hide.bs.modal', function (e) {
                                callback(e);
                            });
                        }
                    }
                };
            },
            dialog: function (options) {
                options = $.extend({}, {
                    title: 'title',
                    url: '',
                    width: 800,
                    height: 550,
                    onReady: function () { },
                    onShown: function (e) { }
                }, options || {});
                var modalId = generateId();

                var content = dialogdHtml.replace(reg, function (node, key) {
                    return {
                        Id: modalId,
                        Title: options.title
                    }[key];
                });
                $('body').append(content);
                var target = $('#' + modalId);
                target.find('.modal-body').load(options.url);
                if (options.onReady())
                    options.onReady.call(target);
                target.modal();
                target.on('shown.bs.modal', function (e) {
                    if (options.onReady(e))
                        options.onReady.call(target, e);
                });
                target.on('hide.bs.modal', function (e) {
                    $('body').find(target).remove();
                });
            }
        }
    }();
})(jQuery);