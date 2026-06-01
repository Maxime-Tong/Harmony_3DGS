/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import vulkan3dgs from 'libvulkan3dgs.so';
import { resourceManager } from '@kit.LocalizationKit';

@Entry
@Component
struct Vulkan3DGSPage {
  private xComponentContext: Record<string, (a: number | boolean | undefined) => void> = {};
  private resourceManager: resourceManager.ResourceManager | undefined;

  aboutToAppear(): void {
    this.resourceManager = this.getUIContext().getHostContext()?.resourceManager;

    try {
      vulkan3dgs.initContext(this.resourceManager);
      console.info('ResourceManager sent to native side.');
    } catch (e) {
      console.error('Failed to init context: ' + JSON.stringify(e));
    }
  }

  build() {
    Column() {
      XComponent({
        id: 'vulkan',
        type: XComponentType.SURFACE,
        libraryname: 'vulkan3dgs'
      })
        .onLoad((xComponentContext?: object | Record<string, (a: number | boolean | undefined) => void>) => {
          if (xComponentContext) {
            this.xComponentContext = xComponentContext as Record<string, (a: number | boolean | undefined) => void>;
            console.info('XComponent loaded successfully');
          } else {
            console.error('Failed to load XComponent context');
          }
        })
        .width('100%')
        .height('100%')
        .backgroundColor(Color.White)

    //   // 按钮区域
    //   Row() {
    //     Button('Start')
    //       .type(ButtonType.Normal)
    //       .fontSize(20)
    //       .fontWeight(FontWeight.Medium)
    //       .backgroundColor('#66BB6A')
    //       .borderRadius(8)
    //       .padding({ left: 24, right: 24, top: 12, bottom: 12 })
    //       .onClick(() => {
    //         this.handleStart();
    //       })
    //
    //     Button('Stop')
    //       .type(ButtonType.Normal)
    //       .fontSize(20)
    //       .fontWeight(FontWeight.Medium)
    //       // 修改背景色为亮红色以便在黑底上可见
    //       .backgroundColor('#EF5350')
    //       .borderRadius(8)
    //       .padding({ left: 24, right: 24, top: 12, bottom: 12 })
    //       .margin({ left: 20 })
    //       .onClick(() => {
    //         this.handleStop();
    //       })
    //   }
    //   .width('100%')
    //   .height('20%')
    //   .justifyContent(FlexAlign.Center)
    //   .alignItems(VerticalAlign.Center)
    }
    .width('100%')
    .height('100%')
    .backgroundColor(Color.White)
  }

  private handleStart(): void {
    try {
      vulkan3dgs.start();
      console.info('vulkan3dgs.start executed.');
    } catch (error) {
      console.error(`Failed to call start: ${JSON.stringify(error)}`);
    }
  }

  private handleStop(): void {
    try {
      vulkan3dgs.stop();
      console.info('vulkan3dgs.stop executed.');
    } catch (error) {
      console.error(`Failed to call stop: ${JSON.stringify(error)}`);
    }
  }
}